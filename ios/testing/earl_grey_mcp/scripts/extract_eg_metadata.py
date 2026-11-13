#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Extracts EarlGrey 2 interface metadata from headers at build time.

Parses EarlGrey helper headers and `*AppInterface` headers to extract:
  - Functional domain groupings
  - Objective-C `@interface` class declarations and class docstrings
  - `#pragma mark` categories
  - Class (`+`) and instance (`-`) method declarations, return types, parameter
    names/types, and docstrings from preceding comments (`//`)
  - Enum definitions (`NS_ENUM`, `NS_OPTIONS`, `enum class`, `enum`)
  - Strict per-method JSON Schemas (`additionalProperties: false`)

Also enforces build-time invariants:
  1. Hierarchy breadth caps (`Domain -> Class -> Category -> Method`) so no
     navigation level or mounted category overloads an LLM agent's context.
  2. Docstring completeness (functionality, backticked parameters, and return
     value documentation).
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path
from typing import Any
import earl_grey_header_util

# Hierarchy breadth caps enforced at build time.
MAX_DOMAINS = 20
MAX_CLASSES_PER_DOMAIN = 25
MAX_CATEGORIES_PER_CLASS = 35
MAX_METHODS_PER_CATEGORY = 35

DEFAULT_CATEGORY_NAME = 'General'

# Regex patterns for parsing Objective-C headers.
# Matches `@interface ClassName` with an optional superclass (`: SuperClass`)
# or category/extension (`(CategoryName)` or `()`).
# Group 1: class name.
# Group 2: superclass name (if subclassing).
# Group 3: category name (empty string for anonymous class extensions).
_INTERFACE_RE = re.compile(
    r'^\s*@interface\s+([A-Za-z_][A-Za-z0-9_]*)'
    r'(?:\s*:\s*([A-Za-z_][A-Za-z0-9_]*)|\s*\(\s*([A-Za-z0-9_]*)\s*\))?'
)

# Marks the closing `@end` boundary of an `@interface` block.
_END_RE = re.compile(r'^\s*@end\b')

# Extracts `#pragma mark` section headers used to group methods into semantic
# categories, stripping any leading Xcode visual separator dash (`-`).
# Group 1: category name (e.g. `Navigation`, `Settings`).
_PRAGMA_MARK_RE = re.compile(r'^\s*#pragma\s+mark\s+(?:-\s*)?(.+?)\s*$')

# Default path to the YAML file defining EarlGrey domain classification rules.
DEFAULT_RULES_PATH = Path(__file__).resolve().parent.parent / 'mcp_domains.yaml'


def is_bridged_earlgrey_header(
    header_path: str | Path,
    rules: tuple[earl_grey_header_util.DomainGroup, ...] | None = None,
) -> bool:
    """Convenience wrapper around `earl_grey_header_util.is_earlgrey_header`."""
    if rules is None:
        rules = earl_grey_header_util.load_domain_rules(DEFAULT_RULES_PATH)
    return earl_grey_header_util.is_earlgrey_header(header_path, rules)


# Enum parsing patterns.

# Matches Apple `NS_ENUM` and `NS_OPTIONS` typedef macros across lines.
# Uses `re.DOTALL` to capture the complete enum body across newlines.
# Group 1: enum type name (the second macro parameter, e.g. `ActionType`).
# Group 2: raw enum body contents between `{` and `}` for member extraction.
_NS_ENUM_RE = re.compile(
    r'typedef\s+NS_(?:ENUM|OPTIONS)\s*\(\s*[^,]+,\s*'
    r'([A-Za-z_][A-Za-z0-9_]*)\s*\)\s*\{([^}]*)\}',
    re.MULTILINE | re.DOTALL,
)

# Matches C++ `enum` and `enum class` definitions across lines, including
# optional underlying type specifications (e.g. `: uint8_t` or `: int`).
# Uses `re.DOTALL` to capture the complete enum body across newlines.
# Group 1: enum type name.
# Group 2: raw enum body contents between `{` and `}`.
_CPP_ENUM_RE = re.compile(
    r'enum\s+(?:class\s+)?([A-Za-z_][A-Za-z0-9_]*)'
    r'(?:\s*:\s*[A-Za-z0-9_: ]+)?\s*\{([^}]*)\}',
    re.MULTILINE | re.DOTALL,
)

# Compatibility aliases for domain classification.
DomainGroup = earl_grey_header_util.DomainGroup


def _extract_param_description(docstring: str, param_name: str) -> str:
    """Extracts the sentence(s) in `docstring` referencing `param_name`."""
    sentences = re.split(r'(?<=[.!?])\s+', docstring.strip())
    pattern = re.compile(
        rf'`{re.escape(param_name)}`'
        rf'|`[^`]*\b{re.escape(param_name)}\b[^`]*`'
        rf'|@param\s+{re.escape(param_name)}\b',
        re.IGNORECASE,
    )
    matching = [s.strip() for s in sentences if pattern.search(s)]
    if matching:
        return ' '.join(matching)
    return f'Parameter `{param_name}` ({docstring.strip()}).'


def _extract_return_description(docstring: str, return_type: str) -> str:
    """Extracts the return value description from `docstring`."""
    if return_type in ('void', 'IBAction'):
        return 'Void (no return value).'
    sentences = re.split(r'(?<=[.!?])\s+', docstring.strip())
    matching = [
        s.strip()
        for s in sentences
        if earl_grey_header_util.RETURN_DOC_RE.search(s)
    ]
    if matching:
        return ' '.join(matching)
    return docstring.strip()


def _schema_for_objc_type(
    objc_type: str,
    enums: dict[str, list[dict[str, Any]]],
) -> dict[str, Any]:
    """Maps a declared Objective-C / C++ type string to a JSON Schema."""
    opt_tok = 'null' + 'able'
    req_tok = 'non' + 'null'
    norm = ' '.join(objc_type.replace(opt_tok, '').replace(req_tok, '').split())
    norm_no_const = re.sub(r'\bconst\b', '', norm).strip()

    # Check if the type matches a known enum.
    bare_type = norm_no_const.split('::')[-1].strip('*& ')
    if enums.get(bare_type):
        enum_names = [entry['name'] for entry in enums[bare_type]]
        return {
            'anyOf': [
                {'type': 'string', 'enum': enum_names},
                {'type': 'integer'},
            ],
            'x_objc_type': objc_type,
            'x_enum_name': bare_type,
        }

    # Boolean types.
    if norm_no_const in ('BOOL', 'bool', 'Boolean'):
        return {'type': 'boolean', 'x_objc_type': objc_type}

    # Integer types.
    if norm_no_const in (
        'int',
        'unsigned int',
        'NSInteger',
        'NSUInteger',
        'int8_t',
        'uint8_t',
        'int16_t',
        'uint16_t',
        'int32_t',
        'uint32_t',
        'int64_t',
        'uint64_t',
        'long',
        'unsigned long',
        'long long',
        'unsigned long long',
        'short',
        'unsigned short',
        'size_t',
    ):
        return {'type': 'integer', 'x_objc_type': objc_type}

    # Floating-point and TimeDelta types.
    if norm_no_const in (
        'float',
        'double',
        'CGFloat',
        'NSTimeInterval',
        'CFTimeInterval',
        'base::TimeDelta',
    ):
        schema: dict[str, Any] = {'type': 'number', 'x_objc_type': objc_type}
        if 'TimeDelta' in norm_no_const:
            schema['description_hint'] = 'Duration in seconds (floating-point).'
        return schema

    # String, URL, and C++ string types.
    if any(
        tok in norm_no_const
        for tok in (
            'NSString',
            'NSMutableString',
            'NSURL',
            'GURL',
            'std::string',
            'std::u16string',
            'char*',
            'char *',
            'SEL',
        )
    ):
        return {'type': 'string', 'x_objc_type': objc_type}

    # Geometry structs represented as objects.
    if norm_no_const in ('CGPoint', 'CGSize', 'CGRect', 'CGVector'):
        if norm_no_const == 'CGPoint':
            return {
                'type': 'object',
                'properties': {
                    'x': {'type': 'number'},
                    'y': {'type': 'number'},
                },
                'required': ['x', 'y'],
                'additionalProperties': False,
                'x_objc_type': objc_type,
            }
        if norm_no_const == 'CGSize':
            return {
                'type': 'object',
                'properties': {
                    'width': {'type': 'number'},
                    'height': {'type': 'number'},
                },
                'required': ['width', 'height'],
                'additionalProperties': False,
                'x_objc_type': objc_type,
            }
        if norm_no_const == 'CGRect':
            return {
                'type': 'object',
                'properties': {
                    'x': {'type': 'number'},
                    'y': {'type': 'number'},
                    'width': {'type': 'number'},
                    'height': {'type': 'number'},
                },
                'required': ['x', 'y', 'width', 'height'],
                'additionalProperties': False,
                'x_objc_type': objc_type,
            }
        return {
            'type': 'object',
            'properties': {'dx': {'type': 'number'}, 'dy': {'type': 'number'}},
            'required': ['dx', 'dy'],
            'additionalProperties': False,
            'x_objc_type': objc_type,
        }

    # Array types.
    if (
        'NSArray' in norm_no_const
        or 'NSMutableArray' in norm_no_const
        or 'std::vector' in norm_no_const
    ):
        return {'type': 'array', 'items': {}, 'x_objc_type': objc_type}

    # Dictionary types.
    if (
        'NSDictionary' in norm_no_const
        or 'NSMutableDictionary' in norm_no_const
    ):
        return {'type': 'object', 'x_objc_type': objc_type}

    # Protocol or object handle types (e.g. id<GREYMatcher>).
    proto_match = re.search(r'<\s*([A-Za-z_][A-Za-z0-9_]*)\s*>', norm_no_const)
    if proto_match:
        type_tag = proto_match.group(1)
        pat = rf'^handle:({re.escape(type_tag)}|[A-Za-z0-9_]+):[0-9]+$'
        return {
            'type': 'string',
            'pattern': pat,
            'x_objc_type': objc_type,
            'x_handle_type': type_tag,
        }

    class_match = re.match(r'^([A-Za-z_][A-Za-z0-9_]*)\s*\*', norm_no_const)
    if class_match:
        type_tag = class_match.group(1)
        pat = rf'^handle:({re.escape(type_tag)}|[A-Za-z0-9_]+):[0-9]+$'
        return {
            'type': 'string',
            'pattern': pat,
            'x_objc_type': objc_type,
            'x_handle_type': type_tag,
        }

    # Generic `id`, enum, or C++ type fallback.
    return {
        'anyOf': [
            {'type': 'string'},
            {'type': 'integer'},
            {'type': 'number'},
            {'type': 'boolean'},
            {'type': 'object'},
            {'type': 'array'},
        ],
        'x_objc_type': objc_type,
    }


# Selectors that are internal singleton accessors or observer registration hooks
# rather than test actuation/observation tools.
_EXCLUDED_SELECTORS: frozenset[str] = frozenset(
    {
        'addObserver:',
        'invokedFromFile:lineNumber:',
        'removeObserver:',
        'setUpHelper',
        'sharedInstance',
        'sharedManager',
    }
)


def is_singleton_helper_class(
    class_name: str, superclass: str | None = None
) -> bool:
    """Returns True if `class_name` is an auto-resolved singleton helper."""
    if superclass == 'BaseEGTestHelperImpl':
        return True
    if class_name in ('AppLaunchManager', 'EarlGreyImpl'):
        return True
    return class_name.endswith(('EarlGreyImpl', 'EarlGreyUIImpl'))


def _should_skip_method(selector: str, params: list[dict[str, str]]) -> bool:
    """Returns True if `selector` is an internal or unbridgeable method."""
    if selector in _EXCLUDED_SELECTORS:
        return True
    return any('FeatureRef' in p['type'] for p in params)


def build_method_tool_schema(
    class_name: str,
    selector: str,
    is_class_method: bool,
    params: list[dict[str, str]],
    enums: dict[str, list[dict[str, Any]]],
    superclass: str | None = None,
) -> dict[str, Any]:
    """Constructs a strict JSON Schema (`additionalProperties: false`)."""
    del selector
    properties: dict[str, Any] = {}
    required: list[str] = []

    if not is_class_method and not is_singleton_helper_class(
        class_name, superclass
    ):
        pat = rf'^handle:({re.escape(class_name)}|[A-Za-z0-9_]+):[0-9]+$'
        properties['target_handle'] = {
            'type': 'string',
            'pattern': pat,
            'description': (
                f'Object handle (`handle:{class_name}:<id>`) for the receiver '
                f'instance of `{class_name}`.'
            ),
        }
        required.append('target_handle')

    for param in params:
        param_name = param['name']
        param_type = param['type']
        # Out-error parameters (`NSError**`) are handled automatically by the
        # runtime dispatcher and omitted from required tool arguments.
        if 'NSError' in param_type and '**' in param_type.replace(' ', ''):
            continue
        prop_schema = dict(_schema_for_objc_type(param_type, enums))
        prop_schema['description'] = param.get(
            'description',
            f'Argument `{param_name}` of type `{param_type}`.',
        )
        properties[param_name] = prop_schema
        required.append(param_name)

    return {
        'type': 'object',
        'properties': properties,
        'required': required,
        'additionalProperties': False,
    }


def extract_enums_from_text(content: str) -> dict[str, list[dict[str, Any]]]:
    """Extracts `NS_ENUM`, `NS_OPTIONS`, and C++ `enum` values."""
    enums: dict[str, list[dict[str, Any]]] = {}
    for regex in (_NS_ENUM_RE, _CPP_ENUM_RE):
        for match in regex.finditer(content):
            enum_name = match.group(1).strip()
            body = match.group(2)
            entries: list[dict[str, Any]] = []
            for raw_line in body.splitlines():
                line = re.sub(r'//.*$', '', raw_line).strip().rstrip(',')
                if not line or line.startswith(('/*', '*')):
                    continue
                if '=' in line:
                    name_part, val_part = line.split('=', 1)
                    entries.append(
                        {
                            'name': name_part.strip(),
                            'value': val_part.strip(),
                        }
                    )
                elif re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', line):
                    entries.append({'name': line, 'value': None})
            if entries:
                enums[enum_name] = entries
    return enums


def _parse_interface_declaration(
    line: str,
    lines: list[str],
    idx: int,
    classes_by_name: dict[str, dict[str, Any]],
    domain_id: str,
    rel_path: str,
) -> tuple[dict[str, Any], str]:
    """Parses an `@interface` line and updates `classes_by_name`.

    Returns `(class_info, category)`.
    """
    iface_match = _INTERFACE_RE.match(line)
    assert iface_match is not None
    class_name = iface_match.group(1)
    superclass = iface_match.group(2)
    category_ext = iface_match.group(3)
    category = category_ext if category_ext else DEFAULT_CATEGORY_NAME
    raw_comments, _ = earl_grey_header_util.extract_preceding_comments(
        lines, idx
    )
    class_doc = earl_grey_header_util.clean_comment_text(raw_comments)

    if class_name not in classes_by_name:
        classes_by_name[class_name] = {
            'class_name': class_name,
            'superclass': superclass,
            'domain': domain_id,
            'header': rel_path,
            'docstring': class_doc,
            'categories': {},
            'methods': {},
        }
    elif superclass and not classes_by_name[class_name]['superclass']:
        classes_by_name[class_name]['superclass'] = superclass
    return classes_by_name[class_name], category


def _parse_method_entry(
    lines: list[str],
    idx: int,
    current_class: dict[str, Any],
    current_category: str,
    enums: dict[str, list[dict[str, Any]]],
    rel_path: str,
    strict_docstrings: bool = True,
) -> tuple[int, list[str]]:
    """Parses a method declaration starting at `idx`.

    Updates `current_class` and returns `(end_idx, errors)`.
    """
    # Collects preceding `//` docstring comments immediately above the method.
    decl_start_line = idx + 1
    raw_comments, _ = earl_grey_header_util.extract_preceding_comments(
        lines, idx
    )
    docstring = earl_grey_header_util.clean_comment_text(raw_comments)

    # Reads lines until the declaration terminates with `;` or `{`.
    full_decl, end_idx = earl_grey_header_util.extract_method_declaration(
        lines, idx
    )
    # Skips initializers or methods explicitly deleted or marked unavailable.
    if 'NS_UNAVAILABLE' in full_decl or '= delete' in full_decl:
        return end_idx, []

    # Parses selector, return type, and parameters into metadata.
    errors: list[str] = []
    try:
        method_info = earl_grey_header_util.parse_method_declaration(full_decl)
    except ValueError as err:
        if strict_docstrings:
            errors.append(f'{rel_path}:{decl_start_line}: {err}')
        return end_idx, errors

    selector = method_info.selector
    is_class_method = method_info.is_class_method
    return_type = method_info.return_type
    params = [
        {
            'label': p.keyword,
            'name': p.param_name,
            'type': p.param_type,
        }
        for p in method_info.params
    ]

    # Filters out internal runtime hooks or unsupported parameter types.
    if _should_skip_method(selector, params):
        return end_idx, errors

    # Enforces that the docstring explains functionality, parameters in
    # backticks, and returns.
    if strict_docstrings:
        val_errors = earl_grey_header_util.validate_method_docstrings(
            method_info, docstring
        )
        for val_err in val_errors:
            errors.append(
                f'{rel_path}:{decl_start_line}: Method `{selector}` '
                f'on `{current_class["class_name"]}`: {val_err}'
            )

    # Extracts per-parameter descriptions from docstring sentences.
    for param in params:
        param['description'] = _extract_param_description(
            docstring, param['name']
        )

    # Constructs the strict JSON Schema for the method tool call.
    return_doc = _extract_return_description(docstring, return_type)
    safe_sel = selector.replace(':', '_')
    tool_name = f'{current_class["class_name"]}__{safe_sel}'
    json_schema = build_method_tool_schema(
        current_class['class_name'],
        selector,
        is_class_method,
        params,
        enums,
        superclass=current_class.get('superclass'),
    )

    # Registers the method under its active `#pragma mark` category.
    categories = current_class['categories']
    categories.setdefault(current_category, [])
    if selector not in categories[current_category]:
        categories[current_category].append(selector)

    # Populates the complete method entry on the current class.
    current_class['methods'][selector] = {
        'selector': selector,
        'tool_name': tool_name,
        'is_class_method': is_class_method,
        'category': current_category,
        'return_type': return_type,
        'return_doc': return_doc,
        'docstring': docstring,
        'params': params,
        'json_schema': json_schema,
    }
    return end_idx, errors


def parse_header_file(
    header_path: Path,
    rel_path: str,
    enums: dict[str, list[dict[str, Any]]],
    strict_docstrings: bool = True,
    domain_id: str = '',
) -> tuple[list[dict[str, Any]], list[str]]:
    """Parses one Objective-C header and returns `(classes, errors)`."""
    text = header_path.read_text(encoding='utf-8')
    lines = text.splitlines()

    errors: list[str] = []
    if not domain_id:
        errors.append(
            f'Header `{rel_path}` cannot be mapped to any known domain in '
            f'`mcp_domains.yaml`. Please add a domain rule for this header.'
        )

    classes_by_name: dict[str, dict[str, Any]] = {}

    in_interface = False
    current_class: dict[str, Any] | None = None
    current_category = DEFAULT_CATEGORY_NAME

    idx = -1
    num_lines = len(lines)
    while idx + 1 < num_lines:
        idx += 1
        line = lines[idx]

        # Detect `@interface`.
        if _INTERFACE_RE.match(line):
            current_class, current_category = _parse_interface_declaration(
                line, lines, idx, classes_by_name, domain_id, rel_path
            )
            in_interface = True
            continue

        if _END_RE.match(line):
            in_interface = False
            current_class = None
            current_category = DEFAULT_CATEGORY_NAME
            continue

        if not in_interface or current_class is None:
            continue

        # Inside `@interface`: check for `#pragma mark`.
        pragma_match = _PRAGMA_MARK_RE.match(line)
        if pragma_match:
            raw_cat = pragma_match.group(1).strip()
            if raw_cat and raw_cat != '-':
                current_category = raw_cat
            continue

        # Method declaration starting with `+` or `-`.
        if earl_grey_header_util.METHOD_START_RE.match(line):
            idx, method_errors = _parse_method_entry(
                lines,
                idx,
                current_class,
                current_category,
                enums,
                rel_path,
                strict_docstrings,
            )
            errors.extend(method_errors)
            continue

    # Filter out empty classes with 0 methods.
    non_empty_classes = [
        cls for cls in classes_by_name.values() if cls['methods']
    ]
    return non_empty_classes, errors


def validate_hierarchy_caps(
    domains: dict[str, dict[str, Any]],
    classes: dict[str, dict[str, Any]],
    max_domains: int = MAX_DOMAINS,
    max_classes_per_domain: int = MAX_CLASSES_PER_DOMAIN,
    max_categories_per_class: int = MAX_CATEGORIES_PER_CLASS,
    max_methods_per_category: int = MAX_METHODS_PER_CATEGORY,
) -> list[str]:
    """Validates breadth caps at every level of the hierarchy."""
    errors: list[str] = []

    if len(domains) > max_domains:
        errors.append(
            f'Hierarchy Breadth Cap Exceeded at Level 1 (Domains): Found '
            f'{len(domains)} functional domains, which exceeds the cap of '
            f'{max_domains}.\n'
            f'  How to rebalance: Merge closely related domain rules in '
            f'`extract_eg_metadata.py` so the top-level domain count stays '
            f'<= {max_domains}.'
        )

    for domain_id, domain_info in sorted(domains.items()):
        domain_classes = domain_info['classes']
        if len(domain_classes) > max_classes_per_domain:
            errors.append(
                f'Hierarchy Breadth Cap Exceeded at Level 2 (Classes in Domain '
                f'`{domain_id}`): Found {len(domain_classes)} classes, which '
                f'exceeds the cap of {max_classes_per_domain}.\n'
                f'  Classes: {", ".join(domain_classes)}\n'
                f'  How to rebalance: Split `{domain_id}` into more specific '
                f'sub-domains in `extract_eg_metadata.py` or merge closely '
                f'related `*AppInterface` classes.'
            )

    for class_name, class_info in sorted(classes.items()):
        header = class_info['header']
        categories = class_info['categories']
        if len(categories) > max_categories_per_class:
            errors.append(
                f'Hierarchy Breadth Cap Exceeded at Level 3 (Categories in '
                f'Class `{class_name}` in `{header}`): Found '
                f'{len(categories)} `#pragma mark` categories, which exceeds '
                f'the cap of {max_categories_per_class}.\n'
                f'  How to rebalance: Consolidate granular `#pragma mark` '
                f'sections in `{header}` or split `{class_name}` into focused '
                f'helper interfaces.'
            )

        for category_name, selectors in sorted(categories.items()):
            if len(selectors) > max_methods_per_category:
                errors.append(
                    f'Hierarchy Breadth Cap Exceeded at Level 4 (Methods in '
                    f'Category `{class_name} > {category_name}` in '
                    f'`{header}`): Found {len(selectors)} methods, which '
                    f'exceeds the cap of {max_methods_per_category}.\n'
                    f'  How to rebalance: Add `#pragma mark - <Subcategory>` '
                    f'annotations in `{header}` to partition '
                    f'`{category_name}` into smaller logical sections '
                    f'(<= {max_methods_per_category} methods each), or move '
                    f'related methods into a dedicated `*AppInterface`.'
                )

    return errors


def extract_metadata(
    src_root: Path,
    header_paths: list[Path] | None = None,
    domain_groups: (dict[str, earl_grey_header_util.DomainGroup] | None) = None,
    rules: tuple[earl_grey_header_util.DomainGroup, ...] | None = None,
    rules_path: Path | None = None,
    max_domains: int = MAX_DOMAINS,
    max_classes_per_domain: int = MAX_CLASSES_PER_DOMAIN,
    max_categories_per_class: int = MAX_CATEGORIES_PER_CLASS,
    max_methods_per_category: int = MAX_METHODS_PER_CATEGORY,
    strict_docstrings: bool = True,
) -> tuple[dict[str, Any], list[str]]:
    """Extracts full metadata dictionary and validation errors."""
    src_root = src_root.resolve()
    all_errors: list[str] = []

    if domain_groups is None:
        if rules is None:
            rules = earl_grey_header_util.load_domain_rules(
                rules_path or DEFAULT_RULES_PATH
            )
        all_domain_groups = earl_grey_header_util.discover_earlgrey_headers(
            src_root, rules=rules
        )
        if header_paths is not None:
            target_set = {h.resolve() for h in header_paths}
            domain_groups = {
                d_id: earl_grey_header_util.DomainGroup(
                    domain_id=d_id,
                    description=g.description,
                    headers=tuple(h for h in g.headers if h in target_set),
                )
                for d_id, g in all_domain_groups.items()
            }
            mapped_headers = {
                h for g in domain_groups.values() for h in g.headers
            }
            for h in header_paths:
                resolved = h.resolve()
                if resolved not in mapped_headers:
                    try:
                        rel = resolved.relative_to(src_root).as_posix()
                    except ValueError:
                        rel = resolved.name
                    all_errors.append(
                        f'Header `{rel}` cannot be mapped to any known '
                        f'domain in `mcp_domains.yaml`. Please add a domain '
                        f'rule for this header.'
                    )
        else:
            domain_groups = all_domain_groups

    enums: dict[str, list[dict[str, Any]]] = {}

    # Pass 1: Extract enums across all headers (plus app_launch_configuration.h
    # for RelaunchPolicy) so schemas can reference them.
    if header_paths is not None:
        enum_headers = list(header_paths)
    else:
        enum_headers = [h for g in domain_groups.values() for h in g.headers]
    launch_cfg = src_root / 'ios/testing/earl_grey/app_launch_configuration.h'
    if launch_cfg.exists():
        enum_headers.append(launch_cfg)
    for header_path in enum_headers:
        resolved = header_path.resolve()
        if resolved.exists():
            enums.update(
                extract_enums_from_text(resolved.read_text(encoding='utf-8'))
            )

    # Pass 2: Parse classes, categories, and methods.

    classes: dict[str, dict[str, Any]] = {}
    domains: dict[str, dict[str, Any]] = {}

    for group in domain_groups.values():
        domain_id = group.domain_id
        domain_desc = group.description
        for header_path in group.headers:
            resolved = header_path.resolve()
            try:
                rel_path = resolved.relative_to(src_root).as_posix()
            except ValueError:
                rel_path = resolved.name

            parsed_classes, errors = parse_header_file(
                resolved,
                rel_path,
                enums,
                strict_docstrings=strict_docstrings,
                domain_id=domain_id,
            )
            all_errors.extend(errors)

            for cls_info in parsed_classes:
                cname = cls_info['class_name']
                if cname not in classes:
                    classes[cname] = cls_info
                else:
                    # Merge categories and methods from category headers.
                    existing = classes[cname]
                    if cls_info['docstring'] and not existing['docstring']:
                        existing['docstring'] = cls_info['docstring']
                    for cat_name, sels in cls_info['categories'].items():
                        existing['categories'].setdefault(cat_name, [])
                        for sel in sels:
                            if sel not in existing['categories'][cat_name]:
                                existing['categories'][cat_name].append(sel)
                    existing['methods'].update(cls_info['methods'])

                domains.setdefault(
                    domain_id,
                    {
                        'domain': domain_id,
                        'description': domain_desc,
                        'classes': [],
                    },
                )
                if cname not in domains[domain_id]['classes']:
                    domains[domain_id]['classes'].append(cname)

    for domain_info in domains.values():
        domain_info['classes'].sort()

    cap_errors = validate_hierarchy_caps(
        domains,
        classes,
        max_domains=max_domains,
        max_classes_per_domain=max_classes_per_domain,
        max_categories_per_class=max_categories_per_class,
        max_methods_per_category=max_methods_per_category,
    )
    all_errors.extend(cap_errors)

    total_methods = sum(len(c['methods']) for c in classes.values())
    metadata = {
        'version': 1,
        'limits': {
            'max_domains': max_domains,
            'max_classes_per_domain': max_classes_per_domain,
            'max_categories_per_class': max_categories_per_class,
            'max_methods_per_category': max_methods_per_category,
        },
        'stats': {
            'num_domains': len(domains),
            'num_classes': len(classes),
            'num_methods': total_methods,
            'num_enums': len(enums),
        },
        'domains': domains,
        'classes': classes,
        'enums': enums,
    }
    return metadata, all_errors


def main(argv: list[str] | None = None) -> int:
    """CLI entrypoint for build-time EarlGrey metadata extraction."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--src-root',
        type=Path,
        required=True,
        help='Path to the Chromium src/ root directory.',
    )
    parser.add_argument(
        '--output',
        type=Path,
        required=True,
        help='Path to the output eg_metadata.json file.',
    )
    parser.add_argument(
        '--headers',
        type=Path,
        nargs='*',
        default=None,
        help='Explicit header paths to parse (defaults to auto-discovery).',
    )
    parser.add_argument(
        '--depfile',
        type=Path,
        default=None,
        help='Optional Ninja depfile path recording all scanned header files.',
    )
    parser.add_argument(
        '--rules',
        '--domains-yaml',
        dest='rules_path',
        type=Path,
        default=DEFAULT_RULES_PATH,
        help='Path to mcp_domains.yaml (defaults to %(default)s).',
    )
    args = parser.parse_args(argv)
    rules = earl_grey_header_util.load_domain_rules(args.rules_path)
    domain_groups = earl_grey_header_util.discover_earlgrey_headers(
        args.src_root, rules=rules
    )
    headers: list[Path]
    if args.headers:
        headers = [h.resolve() for h in args.headers]
        target_set = set(headers)
        domain_groups = {
            d_id: earl_grey_header_util.DomainGroup(
                domain_id=d_id,
                description=g.description,
                headers=tuple(h for h in g.headers if h in target_set),
            )
            for d_id, g in domain_groups.items()
        }
    else:
        headers = [h for g in domain_groups.values() for h in g.headers]

    metadata, errors = extract_metadata(
        src_root=args.src_root,
        domain_groups=domain_groups,
        rules=rules,
        rules_path=args.rules_path,
    )

    if errors:
        sys.stderr.write(
            f'ERROR: EarlGrey MCP metadata extraction failed with '
            f'{len(errors)} error(s):\n\n'
        )
        for err in errors:
            sys.stderr.write(f'  * {err}\n')
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + '\n',
        encoding='utf-8',
    )

    if args.depfile:
        args.depfile.parent.mkdir(parents=True, exist_ok=True)
        all_deps = [str(args.rules_path)] + [str(h) for h in headers]
        dep_inputs = ' '.join(all_deps)
        args.depfile.write_text(
            f'{args.output}: {dep_inputs}\n',
            encoding='utf-8',
        )

    return 0


if __name__ == '__main__':
    sys.exit(main())
