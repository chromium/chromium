# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared utilities for parsing and validating EarlGrey headers.

Provides common path filters, regex patterns, declaration/comment extractors,
and docstring completeness validation shared between `ios/PRESUBMIT.py` and
build-time MCP metadata extraction (`extract_eg_metadata.py`).
"""

from dataclasses import dataclass
from fnmatch import fnmatchcase
from pathlib import Path
import re

try:
    import yaml
except ImportError:
    import pyyaml as yaml


# Detects the beginning of an Objective-C method declaration by matching the
# class (`+`) or instance (`-`) prefix followed by the opening parenthesis of
# the return type.
# Group 1: `+` or `-`.
METHOD_START_RE = re.compile(r'^\s*([+-])\s*\(')

# Parses individual keyword-argument components of multi-part selectors
# (e.g. `withURL:(NSURL*)url` or anonymous first parts like `:(NSString*)str`).
#
# The return/parameter type subpattern `\(((?:[^()]|\([^()]*\))*)\)` uses
# balanced-group simulation to handle one level of nested parentheses (e.g.
# block callbacks `void (^)(BOOL)` or function pointers `void (*)(int)`).
#
# Group 1: selector keyword (optional for anonymous arguments, e.g. `:(id)x`).
# Group 2: argument type inside parentheses.
# Group 3: parameter variable name.
SELECTOR_PART_RE = re.compile(
    r'([A-Za-z_][A-Za-z0-9_]*)?\s*:\s*'
    r'\(((?:[^()]|\([^()]*\))*)\)\s*'
    r'([A-Za-z_][A-Za-z0-9_]*)'
)

# Matches parameterless (unary) methods (e.g. `+ (BOOL)isSignedOut;`).
# Uses the same nested-parentheses pattern for the return type as
# `SELECTOR_PART_RE`.
# Group 1: return type.
# Group 2: unary selector name.
UNARY_SELECTOR_RE = re.compile(
    r'^\s*[+-]\s*\(((?:[^()]|\([^()]*\))*)\)\s*([A-Za-z_][A-Za-z0-9_]*)'
)

# Extracts only the return type enclosed in parentheses directly following the
# `+` or `-` method prefix, tolerating nested parentheses in types.
# Group 1: return type string.
RETURN_TYPE_RE = re.compile(r'^\s*[+-]\s*\(((?:[^()]|\([^()]*\))*)\)')

# Validates docstrings on non-void methods for descriptions explaining the
# return value. Matches explicit markers (`@return`, `returns`) as well as
# implicit outcome indicators (e.g. `whether`, `YES`/`NO`, `nil`, `count`).
RETURN_DOC_RE = re.compile(
    r'(\breturns?\b|@return|\bwhether\b|\byes\b|\bno\b|\btrue\b|\bfalse\b'
    r'|\bnil\b|\bmatcher\b|\baction\b|\bcount\b|\bnumber\s+of\b)',
    re.IGNORECASE,
)

# Strips `//` comment delimiters.
COMMENT_SLASHES_RE = re.compile(r'^///?\s?')

# Identifies placeholder TODO or FIXME comment lines that should not count as
# valid documentation.
TODO_ONLY_RE = re.compile(
    r'^(?:TO' r'DO(?:\([^)]*\))?:?|FIXME:?).*$', re.IGNORECASE
)


@dataclass(frozen=True)
class MethodParam:
    """Represents one parameter in an Objective-C method selector."""

    keyword: str
    param_type: str
    param_name: str


@dataclass(frozen=True)
class MethodInfo:
    """Parsed metadata for an Objective-C method declaration."""

    selector: str
    is_class_method: bool
    return_type: str
    params: list[MethodParam]


@dataclass(frozen=True)
class DomainGroup:
    """Represents an EarlGrey MCP functional domain, its path rules, and
    headers."""

    domain_id: str
    description: str
    includes: tuple[str, ...] = ()
    excludes: tuple[str, ...] = ()
    headers: tuple[Path, ...] = ()


def load_domain_rules(rules_path: Path) -> tuple[DomainGroup, ...]:
    """Loads domain classification rules from a YAML file at `rules_path`.

    Returns:
        A tuple of `DomainGroup` instances containing path patterns and
        metadata.
    """
    with open(rules_path, 'r', encoding='utf-8') as f:
        data = yaml.safe_load(f)

    rules = []
    for item in data.get('domains', []):
        domain_id = item['id']
        description = item['description']
        includes = tuple(item.get('include', item.get('matches', [])))
        excludes = tuple(item.get('exclude', []))
        rules.append(
            DomainGroup(
                domain_id=domain_id,
                description=description,
                includes=includes,
                excludes=excludes,
            )
        )
    return tuple(rules)


def is_earlgrey_header(
    rel_path: str,
    rules: tuple[DomainGroup, ...],
) -> bool:
    """Returns True if `rel_path` matches an EarlGrey MCP domain glob rule."""
    normalized = rel_path.replace('\\', '/').lstrip('/')
    for group in rules:
        if any(fnmatchcase(normalized, e) for e in group.excludes):
            continue

        for pattern in group.includes:
            if fnmatchcase(normalized, pattern):
                return True

    return False


def discover_earlgrey_headers(
    src_root: Path,
    rules: tuple[DomainGroup, ...],
) -> dict[str, DomainGroup]:
    """Finds all bridged EarlGrey and AppInterface headers under `src_root`,
    grouped by domain in a single pass using direct globbing.

    Returns:
        A dict mapping each `domain_id` to its `DomainGroup` containing domain
        metadata and sorted resolved header `Path`s.
    """
    domain_headers: dict[str, list[Path]] = {g.domain_id: [] for g in rules}
    seen: set[Path] = set()

    for group in rules:
        for pattern in group.includes:
            for candidate in src_root.glob(pattern):
                resolved = candidate.resolve()
                if resolved in seen:
                    continue
                rel_path = resolved.relative_to(src_root).as_posix()
                if any(fnmatchcase(rel_path, e) for e in group.excludes):
                    continue
                seen.add(resolved)
                domain_headers[group.domain_id].append(resolved)

    return {
        group.domain_id: DomainGroup(
            domain_id=group.domain_id,
            description=group.description,
            includes=group.includes,
            excludes=group.excludes,
            headers=tuple(sorted(domain_headers[group.domain_id])),
        )
        for group in rules
    }


def strip_trailing_attributes(decl: str) -> str:
    """Removes trailing compiler attributes and semicolon from `decl`."""
    cleaned = re.sub(r'\[\[[^\]]*\]\]', '', decl)
    cleaned = re.sub(r'__attribute__\s*\(\([^)]*\)\)', '', cleaned)
    cleaned = re.sub(
        r'\bNS_[A-Z0-9_]+(?:\(((?:[^()]|\([^()]*\))*)\))?', '', cleaned
    )
    return cleaned.strip().rstrip(';').strip()


def extract_method_declaration(
    lines: list[str], start_line_idx: int
) -> tuple[str, int]:
    """Extracts a multi-line Objective-C method declaration.

    Reads lines starting from `start_line_idx` until a semicolon is encountered
    or end of file. Returns `(full_declaration_string, end_line_idx)`.
    """
    decl_lines = [lines[start_line_idx].strip()]
    current_idx = start_line_idx
    num_lines = len(lines)
    while (
        ';' not in decl_lines[-1]
        and '{' not in decl_lines[-1]
        and current_idx + 1 < num_lines
    ):
        current_idx += 1
        stripped = lines[current_idx].strip()
        decl_lines.append(stripped)
        if stripped.startswith('@end'):
            break
    full_decl = ' '.join(s.strip() for s in decl_lines)
    return full_decl, current_idx


def extract_preceding_comments(
    lines: list[str], start_line_idx: int
) -> tuple[list[str], int]:
    """Collects contiguous preceding `//` comment lines above `start_line_idx`.

    Walks backwards from `start_line_idx - 1`, capturing lines that start with
    `//`. Returns `(comment_lines, comment_start_line_idx)` where
    `comment_lines` is ordered top-down.
    """
    comment_lines: list[str] = []
    current_idx = start_line_idx - 1
    while current_idx >= 0 and lines[current_idx].strip().startswith('//'):
        comment_lines.append(lines[current_idx].strip())
        current_idx -= 1

    comment_start_line_idx = current_idx + 1
    comment_lines.reverse()
    return comment_lines, comment_start_line_idx


def clean_comment_text(comment_lines: list[str]) -> str:
    """Strips `//` comment delimiters and filters placeholder TODO lines.

    Returns the cleaned comment text as a single concatenated string.
    """
    cleaned_comments: list[str] = []
    for raw_comment in comment_lines:
        text = COMMENT_SLASHES_RE.sub('', raw_comment).strip()
        if text and not TODO_ONLY_RE.match(text):
            cleaned_comments.append(text)
    return ' '.join(cleaned_comments)


def parse_method_declaration(full_decl: str) -> MethodInfo:
    """Parses a method declaration string into a structured `MethodInfo`."""
    cleaned = strip_trailing_attributes(full_decl)
    m_start = METHOD_START_RE.match(cleaned)
    if not m_start:
        raise ValueError(f'Invalid method declaration: {full_decl}')

    is_class_method = m_start.group(1) == '+'

    # Extract return type.
    m_ret = RETURN_TYPE_RE.match(cleaned)
    return_type = m_ret.group(1).strip() if m_ret else 'void'

    # Check for unary selector.
    m_unary = UNARY_SELECTOR_RE.match(cleaned)
    if m_unary and ':' not in cleaned:
        return MethodInfo(
            selector=m_unary.group(2).strip(),
            is_class_method=is_class_method,
            return_type=return_type,
            params=[],
        )

    # Multi-part selector.
    raw_params = SELECTOR_PART_RE.findall(cleaned)
    if not raw_params:
        # Fallback if regex didn't extract params but ':' is present.
        colon_idx = cleaned.find(':')
        fallback_name = (
            cleaned[:colon_idx].rsplit(')', 1)[-1].strip() + ':'
            if colon_idx != -1
            else cleaned
        )
        return MethodInfo(
            selector=fallback_name,
            is_class_method=is_class_method,
            return_type=return_type,
            params=[],
        )

    selector = ':'.join((p[0] or '') for p in raw_params) + ':'
    params = [
        MethodParam(
            keyword=kw or '',
            param_type=pt.strip(),
            param_name=pn.strip(),
        )
        for kw, pt, pn in raw_params
    ]
    return MethodInfo(
        selector=selector,
        is_class_method=is_class_method,
        return_type=return_type,
        params=params,
    )


def validate_method_docstrings(
    method_info: MethodInfo,
    doc_text: str,
) -> list[str]:
    """Validates docstrings for method functionality, parameters, and returns.

    Returns a list of error strings (empty if validation passed).
    """
    validation_errors: list[str] = []
    if not doc_text:
        validation_errors.append(
            'Missing docstring describing method functionality '
            '(use `//` comments, not `/* ... */` comment blocks)'
        )
        return validation_errors

    for param in method_info.params:
        # Out-parameter `NSError** error` is standard Objective-C error
        # handling boilerplate and does not require explicit documentation.
        if param.param_name == 'error' or 'NSError' in param.param_type:
            continue
        backtick_pat = r'`' + re.escape(param.param_name) + r'`'
        if not re.search(backtick_pat, doc_text):
            validation_errors.append(
                f'Undocumented parameter `{param.param_name}` (method '
                f'does not document parameter `{param.param_name}` in '
                'backticks)'
            )

    ret_type = method_info.return_type
    # Methods returning `void` (or UI action `IBAction`) have no return value,
    # and initializers returning `instancetype` return the initialized instance
    # by definition, so they do not require return value descriptions.
    if ret_type not in (
        'void',
        'instancetype',
        'IBAction',
    ) and not RETURN_DOC_RE.search(doc_text):
        validation_errors.append(
            f'Missing return value description for `{ret_type}` '
            '(method does not document its return value)'
        )

    return validation_errors
