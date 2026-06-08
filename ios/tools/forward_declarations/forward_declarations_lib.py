# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Library for identifying and applying forward declarations in C++/ObjC."""

import collections
import json
from pathlib import Path
import re
import subprocess
import sys


def find_src_root():
    """Locates the Chromium src root by looking for root markers like .git or .gn."""
    current = Path(__file__).resolve()
    for parent in [current] + list(current.parents):
        if (parent / ".git").exists() or (parent / ".gn").exists():
            return parent

    raise FileNotFoundError(
        "Could not find the Chromium source root. "
        "Please ensure this script is inside a Chromium checkout "
        "containing a .git or .gn file."
    )


# Smart pointer/reference wrappers that allow forward-declared types in headers.
_ALLOWED_TEMPLATES = [
    'unique_ptr', 'shared_ptr', 'weak_ptr', 'scoped_refptr', 'WeakPtr',
    'raw_ptr', 'raw_ref', 'std::unique_ptr', 'std::shared_ptr',
    'std::weak_ptr', 'base::WeakPtr'
]


# Map highly common base/system classes and types to their
# corresponding headers. This prevents removing core headers that
# define typedefs, macros, or templates that cannot be easily parsed
# from direct headers without recursive parsing.
def _load_core_header_mapping():
    mapping_path = Path(__file__).resolve().parent / 'core_header_mapping.json'
    try:
        return json.loads(mapping_path.read_text())
    except Exception as e:
        print(
            f"❌ Error: Failed to load core_header_mapping.json configuration: {e}"
        )
        print(
            "Please verify that the JSON file contains valid syntax "
            "and no trailing commas."
        )
        sys.exit(1)


CORE_HEADER_MAPPING = _load_core_header_mapping()


# Compile at module level
_IMPORT_PATTERN = re.compile(
    r'^\s*(#\s*(?:import|include)\s+["<]([^">]+)[">])', re.MULTILINE)

def find_imports(content):
    """Finds all project-relative imports and includes in the file content."""
    return _IMPORT_PATTERN.findall(content)


def resolve_import_path(import_path, src_root, current_file_path):
    """Resolves an import path to an absolute Path object."""
    for root in [Path(src_root), Path(current_file_path).parent]:
        candidate = root / import_path
        if candidate.exists():
            return candidate.resolve()
    return None


_MASK_PATTERN = re.compile(
    r'''
      //[^\n]*              # Single-line comments (// ...)
      |                     # OR
      /\*[\s\S]*?\*/        # Multi-line comments (/* ... */)
      |                     # OR
      "(?:\\.|[^\\"])*"     # Double-quoted strings ("...")
      |                     # OR
      '(?:\\.|[^\\'])*'     # Single-quoted strings ('...')
    ''',
    re.VERBOSE
)

def mask_non_code(content):
    """Replaces comments and string literals with spaces.

    This ensures that characters like '{', '}', or '#import' inside
    comments or strings do not interfere with the lexical analysis,
    while perfectly preserving the original character offsets.
    """
    if not content:
        return ""
    return _MASK_PATTERN.sub(lambda m: ' ' * len(m.group(0)), content)


# Match Objective-C interface declarations and protocol definitions.
# Group 'class' matches class @interfaces, capturing 'class_name'.
# Group 'proto' matches protocol definitions, capturing 'proto_name'.
_OBJC_PATTERN = re.compile(
    r'(?P<class>@interface\s+(?P<class_name>\w+))|'
    r'(?P<proto>@protocol\s+(?P<proto_name>\w+))'
)

def find_objc_definitions(content):
    """Finds ObjC class and protocol definitions in one pass."""
    clean_content = mask_non_code(content)

    classes = set()
    protocols = set()

    for m in _OBJC_PATTERN.finditer(clean_content):
        if m.group('class'):
            classes.add(m.group('class_name'))
        elif m.group('proto'):
            match_after = re.search(r'\S', clean_content, pos=m.end())
            if match_after and match_after.group(0) != ';':
                protocols.add(m.group('proto_name'))

    return classes, protocols


# Match C++ structural tokens for namespace tracking and class/struct declarations.
# Group 'ns' matches C++ namespaces (including C++17 nested e.g., 'a::b'), capturing 'ns_name'.
# Group 'class' matches 'class' or 'struct' declarations (excluding 'enum class'), capturing 'kind' and 'name'.
# Group 'open' and 'close' match curly braces to track block nesting levels.
_STRUCT_PATTERN = re.compile(
    r'(?P<ns>\bnamespace\s+(?P<ns_name>[a-zA-Z0-9_:]*)\s*\{)|'
    r'(?P<class>\b(?<!enum\s)(?P<kind>class|struct)\s+(?:[A-Z0-9_]+\s+)*(?P<name>\w+)\b)|'
    r'(?P<open>\{)|'
    r'(?P<close>\})'
)

def find_cpp_definitions(content):
    """Finds C++ definitions and their namespaces using a single-pass stack."""
    clean_content = mask_non_code(content)

    definitions = set()
    ns_stack = []  # Tracks (namespace_name, brace_depth) for nested scopes.
    brace_depth = 0  # Current nesting level; ensures namespaces are only closed by their own braces.

    for m in _STRUCT_PATTERN.finditer(clean_content):
        if m.group('ns'):
            name = m.group('ns_name') or "(anonymous)"
            brace_depth += 1
            ns_stack.append((name, brace_depth))

        elif m.group('class'):
            if any(n[0] == "(anonymous)" for n in ns_stack):
                continue

            kind = m.group('kind')
            name = m.group('name')

            match_after = re.search(r'\S', clean_content, pos=m.end())

            if match_after and match_after.group(0) in (':', '{'):
                full_ns = "::".join([n[0] for n in ns_stack])
                definitions.add((name, kind, full_ns or None))

        elif m.group('open'):
            brace_depth += 1

        elif m.group('close'):
            if ns_stack and ns_stack[-1][1] == brace_depth:
                ns_stack.pop()
            brace_depth -= 1

    return definitions


_NON_DECLARABLE_PATTERN = re.compile(
    r'''
      \bNS_(?:CLOSED_)?ENUM\s*\(\s*\w+\s*,\s*(?P<objc_enum>\w+)\)   # ObjC Enum
      |
      \benum\s+(?:class\s+)?(?P<cpp_enum>\w+)\b                     # C++ Enum
      |
      \#\s*define\s+(?P<macro>\w+)\b                                # Macro
      |
      \btypedef\s+[^;]+?\b(?P<tdef>\w+)\s*;                         # Typedef
      |
      \busing\s+(?P<using>\w+)\s*=[^;]+;                            # Using alias
      |
      \btemplate\s*<[^>]*?>\s*(?:class|struct|using)\s+(?P<tmpl>\w+)\b # Template
      |
      \bextern\s+[^;]+?\b(?P<ext>\w+)\s*;                           # Extern constant
      |
      \b(?:const|constexpr)\s+[^=;]+?\b(?P<const>\w+)\s*=           # Const/constexpr
      |
      \binline\s+[^;(]+?\b(?P<inline>\w+)\s*\(                      # Inline function
    ''',
    re.VERBOSE
)

def find_non_forward_declarables(content):
    """Finds all enums, macros, and aliases in one pass."""
    clean_content = mask_non_code(content)

    symbols = set()
    for m in _NON_DECLARABLE_PATTERN.finditer(clean_content):
        for name, value in m.groupdict().items():
            if value:
                symbols.add(value)
    return symbols


def get_usage_context(content):
    """Deep structural analysis of the header to identify how symbols are used.

    Note: This extracts raw tokens without their namespace, which can lead to
    false positives if distinct namespaces define symbols with the same name.
    A future improvement could be to reuse the namespacing stack tracking
    pattern from find_cpp_definitions() to associate tokens with their
    respective namespace scopes.
    """
    clean = mask_non_code(content)

    context = {
        'full_type_required': set(),
        'all_tokens': set(re.findall(r'\b\w+\b', clean))
    }
    # 1. Inheritance: class A : public [B] or @interface A : [B]
    base_blocks = re.findall(
        r'(?:\b(?:class|struct)\s+\w+|@interface\s+\w+)\s*:\s*([^{;]+)', clean)
    for block in base_blocks:
        block_clean = re.sub(r'<[^>]+>', '', block)
        for w in re.findall(r'\b\w+\b', block_clean):
            if w not in ('public', 'protected', 'private', 'virtual'):
                context['full_type_required'].add(w)

    # 2. Obj-C Protocols: @interface A <B, C> or @protocol A <B, C>
    protocols = re.findall(
        r'(?:@interface|@protocol)\b[^{;]*<\s*([^>]+)\s*>', clean)
    for p_list in protocols:
        for w in re.findall(r'\b\w+\b', p_list):
            context['full_type_required'].add(w)

    # 3. Member Access: [B]->, [B]., [B]::
    context['full_type_required'].update(
        re.findall(r'\b(\w+)\s*(?:->|\.|::)', clean))
    # 4. Construction: new [B]
    context['full_type_required'].update(
        re.findall(r'\bnew\s+(\w+)\b', clean))
    # 5. Templates: [B]<T>
    context['full_type_required'].update(
        re.findall(r'\b(\w+)\s*<', clean))
    # 6. Virtual overrides: [B] ... override;
    context['full_type_required'].update(
        re.findall(r'\b(\w+)\b[^;{]*?\b(?:override|final)\b', clean))
    # 7. Value Usage: Types used without * or & (e.g. 'MyType my_var;')
    token_pattern = re.compile(
        r'(?P<prefix>(?:\b\w+\s+){0,3})\b(?P<name>\w+)\b\s*(?P<suffix>[*&]|<)?')
    for m in token_pattern.finditer(clean):
        name = m.group('name')
        prefix = m.group('prefix')
        suffix = m.group('suffix')

        if re.search(r'\b(?:class|struct|@class|@protocol|@interface)\s+$', prefix):
            continue

        if suffix in ('*', '&'):
            continue

        pre_content = clean[:m.start()].rstrip()
        if any(pre_content.endswith(t + '<') or pre_content.endswith(t + ' <')
               for t in _ALLOWED_TEMPLATES):
            continue

        context['full_type_required'].add(name)
    return context


def requires_full_definition(name, context):
    """Checks if the given type requires a full definition in the header."""
    if name in context['full_type_required']:
        return True, "usage requiring full definition detected (inheritance, access, or value usage)"

    return False, None


def analyze_header(header_path, src_root):
    """Analyzes a header file to identify imports that can be forward declared.

    Returns a list of dicts, each representing an import and its analysis.
    """
    header_path = Path(header_path)
    header_content = header_path.read_text(errors='ignore')
    clean_header_content = mask_non_code(header_content)

    usage_context = get_usage_context(clean_header_content)
    header_tokens = usage_context['all_tokens']

    imports = find_imports(clean_header_content)
    results = []

    for full_line, import_path in imports:
        resolved_path = resolve_import_path(import_path, src_root, header_path)
        if not resolved_path:
            results.append({
                'line': full_line,
                'import_path': import_path,
                'status': 'keep',
                'reason': 'Could not resolve import path locally'
            })
            continue

        used_core_types = []
        for type_name, mapped_path in CORE_HEADER_MAPPING.items():
            if import_path == mapped_path:
                clean_type_name = type_name.split('_legacy')[0]
                if clean_type_name in header_tokens:
                    used_core_types.append(clean_type_name)

        if used_core_types:
            results.append({
                'line': full_line,
                'import_path': import_path,
                'status': 'keep',
                'reason': f"Uses core type(s) requiring this header: {', '.join(used_core_types)}"
            })
            continue

        dependency_content = resolved_path.read_text(errors='ignore')

        # 1. Quick Check: Non-declarables (Enums, Macros)
        non_declarables = find_non_forward_declarables(dependency_content)
        used_non_decls = [s for s in non_declarables if s in header_tokens]
        if used_non_decls:
            results.append({
                'line': full_line,
                'import_path': import_path,
                'status': 'keep',
                'reason': f"Uses non-forward-declarable symbols: {', '.join(used_non_decls)}"
            })
            continue

        # 2. Only if safe, do the expensive check for classes
        objc_classes, objc_protocols = find_objc_definitions(dependency_content)
        cpp_defs = find_cpp_definitions(dependency_content)

        all_declarable_types = list(objc_classes) + list(objc_protocols) + [
            d[0] for d in cpp_defs
        ]

        if not all_declarable_types:
            results.append({
                'line': full_line,
                'import_path': import_path,
                'status': 'keep',
                'reason': 'Imported file defines no forward-declarable types (classes, structs, protocols)'
            })
            continue

        forward_decls = []
        can_forward_declare = True
        reasons = []
        used_any = False

        # 1. Check Obj-C Classes
        for cls in objc_classes:
            if cls in usage_context['all_tokens']:
                used_any = True
                must_keep, reason = requires_full_definition(cls, usage_context)
                if must_keep:
                    can_forward_declare = False
                    reasons.append(f"Class '{cls}' {reason}")
                else:
                    forward_decls.append((cls, f"@class {cls};", "objc_class"))

        # 2. Check Obj-C Protocols
        for proto in objc_protocols:
            if proto in usage_context['all_tokens']:
                used_any = True
                must_keep, reason = requires_full_definition(proto, usage_context)
                if must_keep:
                    can_forward_declare = False
                    reasons.append(f"Protocol '{proto}' {reason}")
                else:
                    forward_decls.append((proto, f"@protocol {proto};", "objc_protocol"))

        # 3. Check C++ Classes and Structs
        for name, kind, ns_name in cpp_defs:
            if name in usage_context['all_tokens']:
                used_any = True
                must_keep, reason = requires_full_definition(name, usage_context)
                if must_keep:
                    can_forward_declare = False
                    reasons.append(f"{kind.capitalize()} '{name}' {reason}")
                else:
                    sig = f"namespace {ns_name} {{ {kind} {name}; }}" if ns_name else f"{kind} {name};"
                    forward_decls.append((name, sig, "cpp_" + kind))

        if not used_any:
            results.append({
                'line': full_line,
                'import_path': import_path,
                'status': 'unused',
                'reason': 'No symbols from this import are used in the header file'
            })
        elif can_forward_declare:
            results.append({
                'line': full_line,
                'import_path': import_path,
                'status': 'forward_declare',
                'forward_declarations': sorted(list(set(forward_decls))),
                'reason': 'All used types can be forward declared'
            })
        else:
            results.append({
                'line': full_line,
                'import_path': import_path,
                'status': 'keep',
                'reason': '; '.join(reasons)
            })

    return results


def insert_forward_declarations(content, new_decls):
    """Inserts forward declarations with correct keywords, grouping, and deduplication."""
    # Step 1: Accurate Deduplication
    objc_cls, objc_proto = find_objc_definitions(content)
    cpp_defs = {d[0] for d in find_cpp_definitions(content)}
    existing_types = objc_cls | objc_proto | cpp_defs

    unique_decls = []
    seen_names = set()
    for name, sig, kind in new_decls:
        if name not in existing_types and name not in seen_names:
            unique_decls.append((name, sig, kind))
            seen_names.add(name)

    if not unique_decls:
        return content

    # Step 2: Categorization & Grouping
    cpp_groups = collections.defaultdict(list)  # ns -> [(name, kind)]
    objc_classes = []
    objc_protocols = []
    for name, sig, kind in unique_decls:
        if kind.startswith('cpp_'):
            ns_match = re.match(r'namespace\s+([a-zA-Z0-9_:]+)\s*\{', sig)
            ns_name = ns_match.group(1) if ns_match else None
            cpp_groups[ns_name].append((name, kind.replace('cpp_', '')))
        elif kind == 'objc_class':
            objc_classes.append(name)
        elif kind == 'objc_protocol':
            objc_protocols.append(name)

    # Step 3: Construct the Formatted Blocks
    formatted_blocks = []

    # ObjC Classes: @class A, B, C;
    if objc_classes:
        formatted_blocks.append(f"@class {', '.join(sorted(objc_classes))};")

    # ObjC Protocols: @protocol A, B, C;
    if objc_protocols:
        formatted_blocks.append(f"@protocol {', '.join(sorted(objc_protocols))};")

    # C++ Groups
    for ns in sorted(cpp_groups.keys(), key=lambda x: (x is not None, x or "")):
        items = sorted(cpp_groups[ns])  # Sort by name
        if ns:
            block = [f"namespace {ns} {{"]
            block.extend([f"{kind} {name};" for name, kind in items])
            block.append(f"}}  // namespace {ns}")
            formatted_blocks.append("\n".join(block))
        else:
            formatted_blocks.append("\n".join([f"{kind} {name};" for name, kind in items]))

    new_block_str = "\n\n".join(formatted_blocks)

    # Step 4: Intelligent Placement
    lines = content.splitlines()
    insert_idx = -1
    for i, line in enumerate(lines):
        if re.match(r'^\s*#\s*(?:import|include)\s+', line):
            insert_idx = i

    if insert_idx != -1:
        header = "\n".join(lines[:insert_idx + 1])
        footer = "\n".join(lines[insert_idx + 1:])
        return f"{header}\n\n{new_block_str}\n\n{footer.lstrip()}"
    else:
        return f"{new_block_str}\n\n{content}"


def insert_sorted_import(content, new_import_path, is_objc=True):
    """Inserts an import/include in sorted alphabetical order into the content."""
    import_line = f'#import "{new_import_path}"' if is_objc else f'#include "{new_import_path}"'

    if new_import_path in content:
        return content

    lines = content.splitlines()
    import_indices = []
    for idx, line in enumerate(lines):
        if re.match(r'^\s*#\s*(?:import|include)\s+"([^"]+)"', line):
            import_indices.append(idx)

    if not import_indices:
        return import_line + '\n\n' + content

    inserted = False
    # Always preserve the first import (index 0) if it is the primary header.
    start_idx = 1
    for idx in import_indices[start_idx:]:
        match = re.match(r'^\s*#\s*(?:import|include)\s+"([^"]+)"', lines[idx])
        if match:
            path = match.group(1)
            if new_import_path < path:
                lines.insert(idx, import_line)
                inserted = True
                break

    if not inserted:
        last_idx = import_indices[-1]
        lines.insert(last_idx + 1, import_line)

    return '\n'.join(lines) + '\n'


def heal_misplaced_fwd_decls(content):
    """Shifts orphan ObjC forward-declarations out of C++ namespaces.

    Reconciles mixed language scope constraints by hoisting misplaced
    interfaces cleanly above offending comment/namespace tokens.
    """
    lines = content.split('\n')
    new_lines = []
    i = 0
    while i < len(lines):
        if i < len(lines) and lines[i].strip().startswith('//'):
            comment_block = []
            while i < len(lines) and lines[i].strip().startswith('//'):
                comment_block.append(lines[i])
                i += 1

            ns_match = False
            ns_block = []
            if i < len(lines) and re.match(r'^namespace\s+(\w+)\s*\{[^}]*\}',
                                           lines[i]):
                pass
            elif i < len(lines) and re.match(r'^namespace\s+(\w+)\s*\{',
                                             lines[i]):
                ns_match = True
                ns_block.append(lines[i])
                i += 1
                brace_count = 1
                while i < len(lines) and brace_count > 0:
                    if '{' in lines[i]:
                        brace_count += lines[i].count('{')
                    if '}' in lines[i]:
                        brace_count -= lines[i].count('}')
                    ns_block.append(lines[i])
                    i += 1

            if ns_match and i < len(lines) and (
                    lines[i].strip().startswith('@interface')
                    or lines[i].strip().startswith('@protocol')):
                new_lines.extend(ns_block)
                new_lines.append("")
                new_lines.extend(comment_block)
                new_lines.append(lines[i])
                i += 1
            else:
                new_lines.extend(comment_block)
                if ns_match:
                    new_lines.extend(ns_block)
                if i < len(lines):
                    new_lines.append(lines[i])
                    i += 1
        else:
            new_lines.append(lines[i])
            i += 1

    return "\n".join(new_lines)


def heal_spacing(content):
    """Reconciles code style layout spacings separating import definitions.

    Enforces dynamic empty-line validation checks to guarantee
    pristine aesthetic layouts.
    """
    lines = content.split("\n")
    last_import_idx = -1
    for idx, line in enumerate(lines):
        if re.match(r'^\s*#\s*(?:import|include)\s+["<][^">]+[">]', line):
            last_import_idx = idx

    if last_import_idx != -1 and last_import_idx < len(lines) - 1:
        next_idx = last_import_idx + 1
        while next_idx < len(lines) and not lines[next_idx].strip():
            next_idx += 1

        if next_idx < len(lines):
            if next_idx == last_import_idx + 1:
                lines.insert(next_idx, "")
                content = "\n".join(lines)

    content = re.sub(
        r'((?:#import|#include)\s+["<][^">]+[">]\n)(@class\s+\w+;)', r'\1\n\2',
        content)
    content = re.sub(
        r'((?:#import|#include)\s+["<][^">]+[">]\n)(namespace\s+\w+\s*\{)',
        r'\1\n\2', content)
    return content


def heal_includes_to_imports(content):
    """Converts includes to imports to enforce ObjC++ compilation guidelines."""
    content = re.sub(r'^#include\s+(["<][^">]+[">])',
                     r'#import \1',
                     content,
                     flags=re.MULTILINE)
    return content


def collect_files(paths):
    """Collects distinct absolute Path objects to header files, recursively scanning directories."""
    collected = set()
    for p_str in paths:
        p = Path(p_str).resolve()
        if p.is_dir():
            for item in p.rglob('*.h'):
                if not item.name.endswith('_swift_bridge.h'):
                    collected.add(item)
        elif p.is_file() and p.suffix == '.h' and not p.name.endswith('_swift_bridge.h'):
            collected.add(p)
        elif p_str.endswith('_swift_bridge.h'):
            print(f"Warning: Skipping Swift bridging header: {p_str}")
        else:
            print(f"Warning: Skipping invalid path or non-header file: {p_str}")
    return collected


def collect_git_files(src_root):
    """Collects absolute Path objects to header files modified in the active git branch."""
    modified = set()
    try:
        # 1. Staged and unstaged local changes
        output_local = subprocess.check_output(
            ["git", "diff", "--name-only", "HEAD"],
            cwd=src_root,
            text=True
        )
        modified.update(output_local.splitlines())
    except Exception:
        pass

    try:
        # 2. Branch commits compared to origin/main
        output_branch = subprocess.check_output(
            ["git", "diff", "--name-only", "origin/main...HEAD"],
            cwd=src_root,
            text=True
        )
        modified.update(output_branch.splitlines())
    except Exception:
        pass

    header_files = set()
    for f in modified:
        f_strip = f.strip()
        if not f_strip:
            continue
        p = Path(src_root) / f_strip
        if p.exists() and p.suffix == '.h' and not p.name.endswith('_swift_bridge.h'):
            header_files.add(p.resolve())
    return header_files
