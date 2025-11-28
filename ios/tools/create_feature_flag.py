# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Util to create a feature flag on iOS, by default in
   ios/chrome/browser/shared/public/features."""

import argparse
import os
import re
import subprocess
import sys
import bisect

# The default relative paths to the files that need to be modified.
FEATURES_H_PATH = "ios/chrome/browser/shared/public/features/features.h"
FEATURES_MM_PATH = "ios/chrome/browser/shared/public/features/features.mm"
FLAG_DESCRIPTIONS_H_PATH = ("ios/chrome/browser/flags/"
                            "ios_chrome_flag_descriptions.h")
FLAG_DESCRIPTIONS_CC_PATH = ("ios/chrome/browser/flags/"
                             "ios_chrome_flag_descriptions.cc")
ABOUT_FLAGS_MM_PATH = "ios/chrome/browser/flags/about_flags.mm"
FLAG_METADATA_JSON_PATH = "chrome/browser/flag-metadata.json"
CHROME_VERSION_PATH = "chrome/VERSION"

json5 = None


def setup_json5(src_root):
    """Adds pyjson5 to the Python path and imports it."""
    global json5
    if json5:
        return
    PYJSON5_PATH = os.path.join(src_root, 'third_party', 'pyjson5', 'src')
    sys.path.append(PYJSON5_PATH)
    import json5 as json5_module
    json5 = json5_module


def to_camel_case(snake_str):
    """Converts a snake_case string to CamelCase."""
    components = snake_str.split('_')
    return "".join(x[0].upper() + x[1:] if x else '' for x in components)


def to_kebab_case(camel_str):
    """Converts a CamelCase string to kebab-case, handling acronyms."""
    # Handles cases like "MyURLFeature" -> "MyURL-Feature"
    s1 = re.sub(r'([A-Z]+)([A-Z][a-z])', r'\1-\2', camel_str)
    # Handles cases like "MyURL-Feature" -> "My-URL-Feature"
    s2 = re.sub(r'([a-z\d])([A-Z])', r'\1-\2', s1)
    return s2.lower()


def run_command(command):
    """Executes a shell command and returns its output."""
    try:
        print(f"Executing: {' '.join(command)}")
        result = subprocess.run(command,
                                check=True,
                                text=True,
                                capture_output=True)
        return result.stdout.strip()
    except subprocess.CalledProcessError as e:
        print(f"Error executing command: {' '.join(command)}")
        print(e.stderr)
        sys.exit(1)


def get_gclient_root():
    """Gets the gclient root directory."""
    return run_command(["gclient", "root"])


def modify_file(file_path, modification_function):
    """Reads a file, applies a modification function, and writes it back."""
    with open(file_path, "r") as f:
        content = f.read()
    modified_content = modification_function(content)
    with open(file_path, "w") as f:
        f.write(modified_content)


def get_current_milestone(src_root):
    """Reads the MAJOR version from chrome/VERSION."""
    version_path = os.path.join(src_root, CHROME_VERSION_PATH)
    with open(version_path, "r") as f:
        for line in f:
            if line.startswith("MAJOR="):
                return int(line.strip().split("=")[1])
    print("Error: Could not find MAJOR version in chrome/VERSION")
    sys.exit(1)


def update_features_h(content, feature_name, features_h_path):
    """Adds the feature flag declaration and function prototype to
    features.h."""
    camel_case_feature_name = to_camel_case(feature_name)
    feature_flag_name = f"k{camel_case_feature_name}"
    feature_declaration = (
        f"// Enables the {camel_case_feature_name} feature.\n"
        f"BASE_DECLARE_FEATURE({feature_flag_name});")
    function_declaration = (
        f"// Returns true if the {camel_case_feature_name} feature is "
        f"enabled.\n"
        f"bool Is{camel_case_feature_name}Enabled();")

    added_code = f"\n\n{feature_declaration}\n\n{function_declaration}\n\n"

    guard = features_h_path.upper().replace('/', '_').replace('.', '_') + '_'
    endif_marker = f"#endif  // {guard}"

    last_endif_pos = content.rfind(endif_marker)
    if last_endif_pos != -1:
        content = content[:last_endif_pos] + \
            added_code + content[last_endif_pos:]
    else:
        print(f"Warning: Could not find #endif marker '{endif_marker}' in "
              f"'{features_h_path}'. File not modified.")

    return content


def update_features_mm(content, feature_name):
    """Adds the feature flag definition and function implementation to
    features.mm."""
    camel_case_feature_name = to_camel_case(feature_name)
    feature_flag_name = f"k{camel_case_feature_name}"
    feature_definition = (f'BASE_FEATURE({feature_flag_name}, '
                          f'base::FEATURE_DISABLED_BY_DEFAULT);')
    function_implementation = f"""bool Is{camel_case_feature_name}Enabled() {{
  return base::FeatureList::IsEnabled({feature_flag_name});
}} """

    added_code = f"\n\n{feature_definition}\n\n{function_implementation}\n"

    return content.rstrip() + added_code


def update_flag_descriptions_h(content, feature_name):
    """Adds the feature flag description declarations to
    ios_chrome_flag_descriptions.h."""
    feature_flag_name = f"k{to_camel_case(feature_name)}"
    name_declaration = f"extern const char {feature_flag_name}Name[];"
    description_declaration = (
        f"extern const char {feature_flag_name}Description[];")

    added_code = f"\n{name_declaration}\n{description_declaration}\n"

    namespace_marker = "namespace flag_descriptions {"
    marker_pos = content.find(namespace_marker)
    if marker_pos != -1:
        insertion_point = marker_pos + len(namespace_marker)
        content = content[:insertion_point] + \
            added_code + content[insertion_point:]

    return content


def update_flag_descriptions_mm(content, feature_name):
    """Adds the feature flag description definitions to
    ios_chrome_flag_descriptions.mm."""
    feature_flag_name = f"k{to_camel_case(feature_name)}"
    name_definition = (f'const char {feature_flag_name}Name[] = '
                       f'"{to_camel_case(feature_name)}";')
    description_definition = (f'const char {feature_flag_name}Description[] = '
                              f'"Enables the {feature_name} feature.";')

    added_code = f"\n\n{name_definition}\n{description_definition}"

    namespace_marker = "namespace flag_descriptions {"
    marker_pos = content.find(namespace_marker)
    if marker_pos != -1:
        insertion_point = marker_pos + len(namespace_marker)
        content = content[:insertion_point] + \
            added_code + content[insertion_point:]

    return content


def update_about_flags_mm(content, feature_name):
    """Adds the feature flag to the kFeatureEntries array in
    about_flags.mm, ensuring the previous entry has a trailing comma."""
    feature_flag_name = f"k{to_camel_case(feature_name)}"
    # The new entry itself includes a trailing comma.
    feature_entry = f"""{{"{to_kebab_case(to_camel_case(feature_name))}",
          flag_descriptions::{feature_flag_name}Name,
          flag_descriptions::{feature_flag_name}Description,
          flags_ui::kOsIos,
     FEATURE_VALUE_TYPE({feature_flag_name})
    }},
"""
    array_start_str = ("constexpr auto kFeatureEntries = "
                       "std::to_array<flags_ui::FeatureEntry>({")
    array_start_index = content.find(array_start_str)
    if array_start_index == -1:
        print("Error: Could not find the start of the kFeatureEntries array.")
        sys.exit(1)

    # Find the opening brace of the array
    opening_brace_index = content.find('{', array_start_index)
    if opening_brace_index == -1:
        print("Error: Could not find '{' for kFeatureEntries array.")
        sys.exit(1)

    end_of_array_marker = "});"
    insertion_index = content.find(end_of_array_marker, array_start_index)
    if insertion_index == -1:
        print("Error: Could not find the end of the kFeatureEntries array.")
        sys.exit(1)

    # Find the last closing brace '}' of an entry before the '};'
    last_brace_index = content.rfind('}', opening_brace_index, insertion_index)

    # Check if we found a brace and it's after the array's opening brace
    # (i.e., the array is not empty).
    if last_brace_index > opening_brace_index:
        # We found a previous entry.
        # Check for a comma in the content between it and the insertion point.
        trailing_content = content[last_brace_index + 1:insertion_index]
        if ',' not in trailing_content:
            # No comma found. We need to add one right after the last brace.
            # Rebuild the content:
            # [content_before_brace] + '}' + ',' + [trailing_whitespace] +
            # [new_entry] + [end_marker]
            return (content[:last_brace_index + 1] + ',' + trailing_content +
                    feature_entry + content[insertion_index:])

    # If the array was empty (last_brace_index <= opening_brace_index)
    # or if a comma was already present, just insert the new entry.
    return content[:insertion_index] + feature_entry + content[insertion_index:]


def update_flag_metadata_json(content, feature_name, owner, team_owner,
                              milestone):
    """Adds a new entry to flag-metadata.json by editing the text directly to
    preserve comments."""

    # Use json5 to parse for analysis, not for modification.
    try:
        data = json5.loads(content)
    except Exception as e:
        print(f"Error parsing {FLAG_METADATA_JSON_PATH}: {e}")
        print(
            "Cannot perform update. Please check the file for syntax errors.")
        sys.exit(1)

    kebab_feature_name = to_kebab_case(to_camel_case(feature_name))

    # Check if the flag already exists.
    names = [entry.get("name") for entry in data if entry.get("name")]
    if kebab_feature_name in names:
        print(f"Feature '{kebab_feature_name}' "
              f"already exists in {FLAG_METADATA_JSON_PATH}.")
        return content

    # Determine where to insert.
    sorted_names = sorted(names)
    insertion_index = bisect.bisect_left(sorted_names, kebab_feature_name)

    # Create the new entry text. Note the trailing comma.
    new_entry_text = f"""  {{
    "name": "{kebab_feature_name}",
    "owners": [ "{owner}", "{team_owner}" ],
    "expiry_milestone": {milestone + 5}
  }},\n"""

    lines = content.splitlines(True)

    if insertion_index == len(sorted_names):
        # Appending the new entry. It should be the last one.
        # Find the closing bracket of the main array.
        last_bracket_index = -1
        for i in range(len(lines) - 1, -1, -1):
            if ']' in lines[i]:
                last_bracket_index = i
                break

        if last_bracket_index == -1:
            print("Error: Could not find closing bracket ']' in "
                  f"{FLAG_METADATA_JSON_PATH}.")
            sys.exit(1)

        # Find the last entry's closing brace '}' before the array's
        # closing bracket.
        last_brace_index = -1
        for i in range(last_bracket_index - 1, -1, -1):
            if '}' in lines[i]:
                last_brace_index = i
                break

        if last_brace_index != -1:
            # Ensure the previously last item has a trailing comma.
            line_content = lines[last_brace_index].rstrip()
            if not line_content.endswith(','):
                lines[last_brace_index] = line_content + ',\n'

        # The new entry should not have a trailing comma if it's the last one.
        new_entry_text_no_comma = new_entry_text.rstrip().rstrip(',') + '\n'
        lines.insert(last_bracket_index, new_entry_text_no_comma)

    else:
        # Inserting in the middle.
        next_feature_name = sorted_names[insertion_index]

        # Find the text block of the next feature.
        next_feature_line_index = -1
        for i, line in enumerate(lines):
            if f'"{next_feature_name}"' in line and '"name"' in line:
                next_feature_line_index = i
                break

        if next_feature_line_index == -1:
            print(f"Error: Could not find insertion point for "
                  f"'{kebab_feature_name}' in {FLAG_METADATA_JSON_PATH}.")
            sys.exit(1)

        # Backtrack to find the opening brace '{' of that feature's object.
        insertion_line_index = -1
        for i in range(next_feature_line_index, -1, -1):
            if '{' in lines[i]:
                insertion_line_index = i
                break

        if insertion_line_index == -1:
            print("Error: Could not find object start for feature "
                  f"'{next_feature_name}'.")
            sys.exit(1)

        # Insert the new entry text (with comma) before that object.
        lines.insert(insertion_line_index, new_entry_text)

    return "".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Create a new iOS Chrome feature flag.")
    parser.add_argument("feature_name",
                        help="The name of the feature flag to create.")
    parser.add_argument("--owner",
                        required=True,
                        help="The owner's email for the feature flag.")
    parser.add_argument("--team-owner",
                        required=True,
                        help="The team owner for the feature flag.")
    parser.add_argument(
        "--features-h-path",
        help="Optional: The relative path to the features.h file from the "
        "src root. If provided, the features.mm path will be derived "
        "from it.")
    parser.add_argument(
        "-n",
        "--no-git",
        action="store_true",
        help="Do not perform any git operations (branch, commit, upload).")

    args = parser.parse_args()

    feature_name = args.feature_name
    owner = args.owner
    team_owner = args.team_owner

    if not args.no_git:
        camel_case_feature_name = to_camel_case(feature_name)
        kebab_feature_name = to_kebab_case(camel_case_feature_name)
        branch_name = f"add-{kebab_feature_name}-flag"

        print(f"\nCreating new branch: {branch_name}")
        run_command(["git", "checkout", "-b", branch_name, "origin/main"])

    gclient_root = get_gclient_root()
    src_root = os.path.join(gclient_root, "src")

    # Set up json5 import.
    setup_json5(src_root)

    # Get current milestone.
    current_milestone = get_current_milestone(src_root)

    # Determine paths for features.h and features.mm
    if args.features_h_path:
        features_h_rel_path = args.features_h_path
        features_mm_rel_path = os.path.splitext(features_h_rel_path)[0] + ".mm"
    else:
        features_h_rel_path = FEATURES_H_PATH
        features_mm_rel_path = FEATURES_MM_PATH

    # Construct absolute paths
    features_h = os.path.join(src_root, features_h_rel_path)
    features_mm = os.path.join(src_root, features_mm_rel_path)
    flag_descriptions_h = os.path.join(src_root, FLAG_DESCRIPTIONS_H_PATH)
    flag_descriptions_mm = os.path.join(src_root, FLAG_DESCRIPTIONS_CC_PATH)
    about_flags_mm = os.path.join(src_root, ABOUT_FLAGS_MM_PATH)
    flag_metadata_json = os.path.join(src_root, FLAG_METADATA_JSON_PATH)

    modified_files = [
        features_h,
        features_mm,
        flag_descriptions_h,
        flag_descriptions_mm,
        about_flags_mm,
        flag_metadata_json,
    ]

    # Modify the files
    modify_file(
        features_h,
        lambda c: update_features_h(c, feature_name, features_h_rel_path))
    modify_file(features_mm, lambda c: update_features_mm(c, feature_name))
    modify_file(flag_descriptions_h,
                lambda c: update_flag_descriptions_h(c, feature_name))
    modify_file(flag_descriptions_mm,
                lambda c: update_flag_descriptions_mm(c, feature_name))
    modify_file(about_flags_mm,
                lambda c: update_about_flags_mm(c, feature_name))
    modify_file(
        flag_metadata_json,
        lambda c: update_flag_metadata_json(c, feature_name, owner, team_owner,
                                            current_milestone),
    )

    print("\nSorting feature flags...")
    order_flags_script = os.path.join(src_root, "ios", "tools",
                                      "order_flags.py")
    # The order_flags.py script returns a non-zero exit code if files were
    # sorted. This is expected, so we run it without `check=True` and
    # simply print its output.
    result = subprocess.run(["python3", order_flags_script],
                            capture_output=True,
                            text=True)
    if result.stdout:
        print(result.stdout.strip())
    if result.stderr:
        # Also print stderr in case of a real error in the script.
        print(result.stderr.strip(), file=sys.stderr)

    if not args.no_git:
        commit_message = f"[iOS]: Add {feature_name} feature flag"

        print("\nFormatting code...")
        run_command(["git", "cl", "format"])

        print("\nAdding modified files to git...")
        run_command(["git", "add"] + modified_files)

        print("\nCommitting changes...")
        run_command(["git", "commit", "-m", commit_message])

        print("\nUploading for review...")
        run_command(["git", "cl", "upload", "-f"])

    print("\nFeature flag created successfully!")


if __name__ == "__main__":
    main()
