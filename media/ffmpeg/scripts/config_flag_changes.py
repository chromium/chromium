# Copyright 2024 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
from robo_lib import shell


def extract_lines_per_file(lines):
    definition_pattern = re.compile(r'^[+-].*[01]$')
    filemap = {}
    current_file = None
    for line in lines:
        if match := definition_pattern.match(line):
            assert current_file is not None
            replaced = line.replace('#define', '').replace('%define', '')
            filemap[current_file].add(replaced)
        if line.startswith('--- a/'):
            current_file = line[6:]
            filemap[current_file] = set()
    return filemap


def get_config_flag_changes(cfg):
    command = [
        'git', 'diff',
        cfg.origin_merge_base(), '--unified=0', '--', 'chromium/config/*'
    ]

    lines = shell.output_or_error(command).split('\n')
    filemapped_deltas = extract_lines_per_file(lines)

    # TODO(liberato) remove this on the next possible roll.
    for file in filemapped_deltas.keys():
        # When the mips configs were deleted, they poisoned the config flag deltas.
        if file.endswith('mips64el/config.h'):
            filemapped_deltas[file] = set()
        if file.endswith('mipsel/config.h'):
            filemapped_deltas[file] = set()

    recombined = set()
    for file, deltas in filemapped_deltas.items():
        recombined.update(deltas)

    recombined = sorted(list(recombined))
    return recombined
