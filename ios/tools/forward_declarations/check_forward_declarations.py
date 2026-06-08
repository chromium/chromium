# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""CLI tool to check which imports in header files can be forward declared."""

import argparse
import sys

import forward_declarations_lib


def main():
    """CLI tool entry point to verify which imports in header files can be forward-declared."""
    # Ensure compatibility by blocking execution on unsupported
    # host environments.
    if sys.platform == 'win32':
        print("Error: This script is not supported on Windows.")
        return 1

    parser = argparse.ArgumentParser(
        description=
        "Check if imported classes in a .h file can be forward-declared.")
    parser.add_argument('paths',
                        nargs='*',
                        help="Paths to header files or directories to scan.")
    parser.add_argument(
        '--git',
        action='store_true',
        help="Automatically scan files modified in the active git branch."
    )
    parser.add_argument(
        '--exit-zero',
        action='store_true',
        help=
        "Always exit with status 0 even if optimization opportunities "
        "are found."
    )
    args = parser.parse_args()

    try:
        src_root = forward_declarations_lib.find_src_root()
    except FileNotFoundError as e:
        print(f"Error: {e}")
        return 1

    # Accumulate distinct header paths from input arguments (files and folders) or git modifications
    if args.git:
        files_to_scan = forward_declarations_lib.collect_git_files(src_root)
    else:
        if not args.paths:
            parser.error("Please specify at least one path or use the --git option.")
        files_to_scan = forward_declarations_lib.collect_files(args.paths)

    if not files_to_scan:
        print("No header files found to scan.")
        return 0

    print(f"Scanning {len(files_to_scan)} header files...\n")

    # Track optimization statistics across the entire scanned batch
    total_files_with_issues = 0
    total_forward_declarable = 0
    total_unused = 0

    # Process and display results in deterministic sorted order
    for file_path in sorted(files_to_scan):
        rel_path = file_path.relative_to(src_root)
        # Execute deep lexical analysis via core library functions
        analysis = forward_declarations_lib.analyze_header(file_path, src_root)

        # Filter items tagged for removal/optimization
        issues = [
            a for a in analysis if a['status'] in ('forward_declare', 'unused')
        ]
        if not issues:
            continue

        total_files_with_issues += 1
        # Output header block highlighted cleanly in terminal blue
        print(f"\033[1;34m[{rel_path}]\033[0m")

        for item in analysis:
            status = item['status']
            if status == 'forward_declare':
                total_forward_declarable += 1
                # Output green recommended forward-declaration line items
                print(f"  \033[1;32m[FORWARD DECLARE]\033[0m {item['line']}")
                print(f"    -> Reason: {item['reason']}")
                print(
                    f"    -> Suggested decls: {', '.join([decl[1] for decl in item['forward_declarations']])}"
                )
            elif status == 'unused':
                total_unused += 1
                # Output yellow flagged unused import items
                print(f"  \033[1;33m[UNUSED]\033[0m {item['line']}")
                print(f"    -> Reason: {item['reason']}")
        print()

    # Output final aggregated statistics summary
    print("-" * 60)
    print(f"Scan complete.")
    print(
        f"Files with issues: {total_files_with_issues} / {len(files_to_scan)}")
    print(f"Imports that can be forward declared: {total_forward_declarable}")
    print(f"Imports that are completely unused: {total_unused}")

    # Suggest bulk fixing CLI path if issues are discovered
    if total_files_with_issues > 0:
        print("\nTo automatically apply forward declarations, run:")
        print(
            f"  python3 ios/tools/forward_declarations/fix_forward_declarations.py <file.h>"
        )
        return 0 if args.exit_zero else 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
