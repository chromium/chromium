# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for magi-mode.

This script enforces structural integrity and formatting for the MAGI protocol
markdown documentation and persona cheat sheets.
"""

import collections
import os
import re

# Regex for tokens exempt from the 80-character line limit.
# Group 1: Absolute src/... paths
# Group 2: Standard URLs (excludes trailing punctuation common in markdown)
# Group 3: Relative markdown links (must not start with src/ or http)
EXEMPT_TOKENS_RE = re.compile(
    r'(src/[a-zA-Z0-9_/\.\-]+)|'
    r'(https?://[^\s()<>\[\]]+[^\s()<>\[\].:;?])|'
    r'\[[^\]]+\]\((?!(?:https?://|src/))([^)#\s]+\.md)[^)]*\)')

# Maximum size for a markdown file to prevent DoS via OOM.
MAX_MD_FILE_SIZE = 1 * 1024 * 1024  # 1MB


def _IsSafePath(input_api, path, root):
  """Normalizes and validates that a path is within a specific root."""
  norm_path = input_api.os_path.normpath(path)
  if norm_path == root:
    return True
  return norm_path.startswith(root + input_api.os_path.sep)


def CheckMarkdownFiles(input_api, output_api):
  results = []
  repo_root = input_api.change.RepositoryRoot()
  magi_dir = input_api.PresubmitLocalPath()

  def FileFilter(affected_file):
    return input_api.FilterSourceFile(
        affected_file,
        files_to_check=(r'.*\.md$',),
    )

  # 1. Map affected files by absolute path for O(1) lookups.
  affected_files_map = {
      f.AbsoluteLocalPath(): f
      for f in input_api.AffectedFiles(
          file_filter=FileFilter, include_deletes=True
      )
  }

  if not affected_files_map:
    return []

  # 2. Identify all markdown files in the directory for reachability.
  all_markdown_files = set()
  for root, _, files in os.walk(magi_dir):
    for file in files:
      if file.endswith('.md'):
        abs_path = input_api.os_path.normpath(
            input_api.os_path.join(root, file)
        )
        # Skip files that are currently being deleted.
        if (
            abs_path in affected_files_map
            and affected_files_map[abs_path].Action() == 'D'
        ):
          continue
        all_markdown_files.add(abs_path)

  # 3. Process every file to build the graph and check formatting.
  # Adjacency list: node (abs path) -> list of connected nodes (abs paths)
  graph = {node: [] for node in all_markdown_files}
  checked_existence = {}

  for md_file in all_markdown_files:
    if input_api.os_path.getsize(md_file) > MAX_MD_FILE_SIZE:
      results.append(
          output_api.PresubmitError(
              f'File {md_file} exceeds max size 1MB (DoS mitigation).'
          )
      )
      continue

    is_modified = md_file in affected_files_map
    content = None

    if is_modified:
      content = input_api.ReadFile(affected_files_map[md_file])
    else:
      try:
        with open(md_file, 'r', encoding='utf-8') as f:
          content = f.read()
      except IOError:
        continue

    if not content:
      continue

    # Scenario 2: Trailing Newlines (Modified files only)
    if is_modified:
      if (
          not content.endswith('\n')
          or content.endswith(('\n\n', '\r\n\r\n'))
          or '\r' in content
      ):
        results.append(
            output_api.PresubmitError(
                f'File {affected_files_map[md_file].LocalPath()} must use '
                'Unix line endings (\\n) and end with exactly one newline.'
            )
        )

    lines = content.splitlines(True)
    in_fenced_block = False
    in_indented_block = False
    prev_line_empty = True

    for line_num, line in enumerate(lines, start=1):
      line_stripped = line.rstrip('\r\n')

      # Fenced code block detection
      if line_stripped.lstrip().startswith(('```', '~~~')):
        in_fenced_block = not in_fenced_block
        continue

      # Indented code block detection
      if not line_stripped.strip():
        prev_line_empty = True
        continue

      if prev_line_empty and (line.startswith('    ') or line.startswith('\t')):
        in_indented_block = True
      elif not (line.startswith('    ') or line.startswith('\t')):
        in_indented_block = False

      prev_line_empty = False

      if in_fenced_block or in_indented_block:
        continue

      # Scenario 1: 80-Character Limit (Modified files only)
      if is_modified:
        line_len = len(line_stripped)
        for match in EXEMPT_TOKENS_RE.finditer(line_stripped):
          line_len -= len(match.group(0))
        if line_len > 80:
          results.append(
              output_api.PresubmitPromptWarning(
                  f'Line {line_num} in '
                  f'{affected_files_map[md_file].LocalPath()} '
                  f'exceeds 80 characters ({len(line_stripped)} chars):\n'
                  f'{line_stripped}'
              )
          )

      # Scenario 3: Link Extraction & Validation
      for match in EXEMPT_TOKENS_RE.finditer(line):
        full_path = None
        token = match.group(0)

        # Group 1: Absolute src/...
        if match.group(1):
          token = match.group(1).rstrip('.')
          full_path = input_api.os_path.normpath(
              input_api.os_path.join(repo_root, token[4:])
          )
        # Group 3: Relative link
        elif match.group(3):
          token = match.group(3)
          full_path = input_api.os_path.normpath(
              input_api.os_path.join(input_api.os_path.dirname(md_file), token)
          )

        if not full_path:
          continue

        # Security: Prevent path traversal
        if not _IsSafePath(input_api, full_path, repo_root):
          msg = (
              f'Line {line_num} in '
              f'{input_api.os_path.relpath(md_file, repo_root)} '
              f'attempts path traversal: {token}'
          )
          if is_modified:
            results.append(output_api.PresubmitError(msg))
          else:
            results.append(output_api.PresubmitPromptWarning(msg))
          continue

        # Add to reachability graph (only for local markdown files)
        if full_path.endswith('.md') and _IsSafePath(
            input_api, full_path, magi_dir
        ):
          graph[md_file].append(full_path)

        # Existence Check (Validate EVERY link in graph to catch
        # deletion-breaks)
        if full_path not in checked_existence:
          checked_existence[full_path] = input_api.os_path.exists(full_path)

        if not checked_existence[full_path]:
          is_active = is_modified or full_path in affected_files_map
          msg = (
              f'Line {line_num} in '
              f'{input_api.os_path.relpath(md_file, repo_root)} '
              f'references a non-existent file: {token}'
          )
          if is_active:
            results.append(output_api.PresubmitError(msg))
          else:
            results.append(output_api.PresubmitPromptWarning(msg))

  # Scenario 4: Reachability (BFS from SKILL.md)
  skill_md_path = input_api.os_path.normpath(
      input_api.os_path.join(magi_dir, 'SKILL.md')
  )
  if skill_md_path not in graph:
    results.append(
        output_api.PresubmitError(
            f'Critical Error: Entry point {skill_md_path} is missing.'
        )
    )
    return results

  visited = set()
  queue = collections.deque([skill_md_path])
  while queue:
    node = queue.popleft()
    if node not in visited:
      visited.add(node)
      for neighbor in graph.get(node, []):
        if neighbor in all_markdown_files and neighbor not in visited:
          queue.append(neighbor)

  for md_file in all_markdown_files:
    if md_file not in visited:
      rel_path = input_api.os_path.relpath(md_file, repo_root)
      is_active_violation = md_file in affected_files_map

      msg = (
          f'Unreachable Markdown File: {rel_path} cannot be reached '
          'from SKILL.md. Even if it links to another file, it is '
          'part of an isolated cycle. Please add a link to it in '
          'PERSONAS.md or another connected document.'
      )

      if is_active_violation:
        results.append(output_api.PresubmitError(msg))
      else:
        results.append(output_api.PresubmitPromptWarning(msg))

  # Scenario 5: Content mandates
  if skill_md_path in affected_files_map:
    skill_content = input_api.ReadFile(affected_files_map[skill_md_path])
    if 'TONE MANDATE (SIGNAL-TO-NOISE):' not in skill_content:
      results.append(
          output_api.PresubmitError(
              'File SKILL.md must contain the "TONE MANDATE (SIGNAL-TO-NOISE):"'
              ' section.'
          )
      )
    elif (
        'Zero Preamble/Postamble' not in skill_content
        or 'Artifacts Only' not in skill_content
    ):
      results.append(
          output_api.PresubmitError(
              'File SKILL.md TONE MANDATE must explicitly enforce '
              '"Zero Preamble/Postamble" and "Artifacts Only".'
          )
      )

  return results


def CheckJsonFiles(input_api, output_api):
  import json

  results = []

  magi_dir = input_api.PresubmitLocalPath()
  schema_path = input_api.os_path.join(magi_dir, 'magi_schema.json')

  schema_content_str = None
  for f in input_api.AffectedFiles(include_deletes=False):
    if f.AbsoluteLocalPath() == schema_path:
      schema_content_str = input_api.ReadFile(f)
      break

  if schema_content_str is None:
    try:
      with open(schema_path, 'r', encoding='utf-8') as f:
        schema_content_str = f.read()
    except IOError:
      pass

  if not schema_content_str:
    return []

  try:
    schema = json.loads(schema_content_str)
  except ValueError as e:
    results.append(output_api.PresubmitError(f'Invalid magi_schema.json: {e}'))
    return results

  if not isinstance(schema, dict):
    results.append(
        output_api.PresubmitError('magi_schema.json must be a JSON object.')
    )
    return results

  state_block_schema = schema.get('definitions', {}).get('StateBlock', {})
  project_spec_schema = schema.get('definitions', {}).get('ProjectSpec', {})
  review_feedback_schema = schema.get('definitions', {}).get(
      'ReviewFeedback', {}
  )
  constraints_schema = schema.get('definitions', {}).get('Constraints', {})
  persona_def_schema = schema.get('definitions', {}).get('PersonaDef', {})

  def FileFilter(affected_file):
    return input_api.FilterSourceFile(
        affected_file,
        files_to_check=(
            r'.*(state_block|project|review(\..+)?|constraints)'
            r'\.magi(\.\d+)?\.json$',
            r'.*personas/.*\.json$',
        ),
    )

  for f in input_api.AffectedFiles(file_filter=FileFilter):
    content_str = input_api.ReadFile(f)
    if not content_str.strip():
      continue
    try:
      content = json.loads(content_str)
    except ValueError as e:
      results.append(
          output_api.PresubmitError(
              f'File {f.LocalPath()} is not valid JSON: {e}'
          )
      )
      continue

    if not isinstance(content, dict):
      results.append(
          output_api.PresubmitError(
              f'File {f.LocalPath()} must be a JSON object.'
          )
      )
      continue

    filename = input_api.os_path.basename(f.LocalPath())
    if filename.startswith('state_block'):
      active_schema = state_block_schema
    elif filename.startswith('project'):
      active_schema = project_spec_schema
    elif filename.startswith('review'):
      active_schema = review_feedback_schema
    elif filename.startswith('constraints'):
      active_schema = constraints_schema
    elif 'personas' in f.LocalPath().replace('\\', '/').split('/'):
      active_schema = persona_def_schema
    else:
      continue

    required_keys = set(active_schema.get('required', []))
    properties = active_schema.get('properties', {})

    missing_keys = required_keys - set(content.keys())
    if missing_keys:
      results.append(
          output_api.PresubmitError(
              f'File {f.LocalPath()} is missing required keys: '
              f"{', '.join(sorted(missing_keys))}"
          )
      )

    # Validating simple type properties of top-level JSON
    for key, value in content.items():
      if key in properties:
        expected_type = properties[key].get('type')
        if expected_type == 'integer' and type(value) is not int:
          results.append(
              output_api.PresubmitError(
                  f"File {f.LocalPath()} key '{key}' should be integer."
              )
          )
        elif expected_type == 'boolean' and not isinstance(value, bool):
          results.append(
              output_api.PresubmitError(
                  f"File {f.LocalPath()} key '{key}' should be boolean."
              )
          )
        elif expected_type == 'array' and not isinstance(value, list):
          results.append(
              output_api.PresubmitError(
                  f"File {f.LocalPath()} key '{key}' should be array."
              )
          )
        elif expected_type == 'string' and not isinstance(value, str):
          results.append(
              output_api.PresubmitError(
                  f"File {f.LocalPath()} key '{key}' should be string."
              )
          )

        # Generic enum validation
        expected_enum = properties[key].get('enum')
        if expected_enum and value not in expected_enum:
          results.append(
              output_api.PresubmitError(
                  f"File {f.LocalPath()} key '{key}' must be one of "
                  f'{expected_enum}.'
              )
          )

    # 4. Decision Graph Validation
    review_mode = content.get('review_mode')
    next_p = content.get('next_phase')

    if filename.startswith('state_block'):
      if next_p and next_p not in [
          'CRITIQUE',
          'SCAFFOLDING',
          'PREPARATION',
          'IMPLEMENTATION',
          'SYNTHESIS',
          'TRAINING',
          'DEADLOCK',
      ]:
        results.append(
            output_api.PresubmitError(
                f'File {f.LocalPath()} has invalid next_phase for '
                f'state block: {next_p}'
            )
        )

      # Cross-file validation for state_transport
      state_transport = content.get('state_transport')
      if state_transport in ('EPHEMERAL', 'EPHEMERAL_WITH_LOGS'):
        project_file_path = input_api.os_path.join(
            input_api.os_path.dirname(f.AbsoluteLocalPath()),
            'project.magi.json',
        )
        proj_content_str = None
        for af in input_api.AffectedFiles(include_deletes=False):
          if af.AbsoluteLocalPath() == project_file_path:
            proj_content_str = input_api.ReadFile(af)
            break

        if not proj_content_str and input_api.os_path.exists(project_file_path):
          try:
            with open(project_file_path, 'r', encoding='utf-8') as proj_f:
              proj_content_str = proj_f.read()
          except IOError:
            pass

        if proj_content_str:
          try:
            proj_content = json.loads(proj_content_str)
            if proj_content.get('paranoia_mode') is True:
              results.append(
                  output_api.PresubmitError(
                      f'File {f.LocalPath()} has state_transport '
                      f'{state_transport} but project.magi.json has '
                      'paranoia_mode: true.'
                  )
              )
            if (
                state_transport == 'EPHEMERAL'
                and proj_content.get('auditability_level') == 'VERBOSE'
            ):
              results.append(
                  output_api.PresubmitError(
                      f'File {f.LocalPath()} has state_transport EPHEMERAL '
                      'but project.magi.json has auditability_level: VERBOSE. '
                      'Consider using EPHEMERAL_WITH_LOGS instead.'
                  )
              )
          except ValueError:
            pass
    elif filename.startswith('project'):
      if next_p and next_p != 'SCAFFOLDING':
        results.append(
            output_api.PresubmitError(
                f'File {f.LocalPath()} must signal next_phase: SCAFFOLDING'
            )
        )
      if 'environment' in content:
        environment = content['environment']
        if not isinstance(environment, dict):
          results.append(
              output_api.PresubmitError(
                  f'File {f.LocalPath()} key "environment" must be an object.'
              )
          )
        else:
          vcs = environment.get('vcs')
          harness = environment.get('harness')
          if vcs not in ('GIT', 'JJ'):
            results.append(
                output_api.PresubmitError(
                    f'File {f.LocalPath()} environment.vcs must be GIT or JJ, '
                    f'got {vcs}'
                )
            )
          if harness not in ('JETSKI', 'GENERIC_CLI'):
            results.append(
                output_api.PresubmitError(
                    f'File {f.LocalPath()} environment.harness must be '
                    f'JETSKI or GENERIC_CLI, got {harness}'
                )
            )
    elif filename.startswith('review'):
      if next_p and next_p != 'ANALYSIS':
        results.append(
            output_api.PresubmitError(
                f'File {f.LocalPath()} must signal next_phase: ANALYSIS'
            )
        )
    elif filename.startswith('constraints'):
      if review_mode == 'SUPERVISOR':
        if next_p and next_p not in ['SYNTHESIS', 'TRAINING']:
          results.append(
              output_api.PresubmitError(
                  f'File {f.LocalPath()} (SUPERVISOR) must signal '
                  f'SYNTHESIS or TRAINING, not {next_p}'
              )
          )
      elif review_mode == 'CONSENSUS':
        if next_p and next_p != 'TPM_UPDATE':
          results.append(
              output_api.PresubmitError(
                  f'File {f.LocalPath()} (CONSENSUS) must signal '
                  f'TPM_UPDATE, not {next_p}'
              )
          )

  return results


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(CheckMarkdownFiles(input_api, output_api))
  results.extend(CheckJsonFiles(input_api, output_api))
  return results


def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(CheckMarkdownFiles(input_api, output_api))
  results.extend(CheckJsonFiles(input_api, output_api))
  return results
