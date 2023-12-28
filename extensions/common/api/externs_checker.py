# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class ExternsChecker(object):
  _UPDATE_MESSAGE = """To update the externs, run:
 src/ $ python3 tools/json_schema_compiler/compiler.py\
 %s --root=. --generator=externs > %s"""

  def __init__(self, input_api, output_api, api_pairs=None, api_root=None):
    if api_pairs is None:
      api_pairs = self.ParseApiFileList(input_api, api_root=api_root)

    self._input_api = input_api
    self._output_api = output_api
    self._api_pairs = api_pairs

    for path in list(api_pairs.keys()) + list(api_pairs.values()):
      if not input_api.os_path.exists(path):
        raise OSError('Path Not Found: %s' % path)

  @staticmethod
  def ParseApiFileList(input_api, api_root=None):
    """Extract the API pairs from the registered list."""
    if api_root is None:
      api_root = input_api.PresubmitLocalPath()
    externs_root = input_api.os_path.join(
        input_api.change.RepositoryRoot(), 'third_party', 'closure_compiler',
        'externs')

    ret = {}
    listing = input_api.os_path.join(api_root, 'generated_externs_list.txt')
    for line in input_api.ReadFile(listing).splitlines():
      # Skip blank & comment lines.
      if not line.split('#', 1)[0].strip():
        continue

      source = input_api.os_path.join(api_root, line)
      api_name, ext = line.rsplit('.', 1)
      assert ext == 'json' or ext == 'idl'
      externs = input_api.os_path.join(externs_root, api_name + '.js')
      ret[source] = externs

    assert ret
    return ret

  def RunChecks(self):
    bad_files = []
    affected = [f.AbsoluteLocalPath() for f in
                   self._input_api.change.AffectedFiles()]
    for path in affected:
      pair = self._api_pairs.get(path)
      if pair != None and pair not in affected:
        bad_files.append({'source': path, 'extern': pair})
    results = []
    if bad_files:
      replacements = (('<source_file>', '<output_file>') if len(bad_files) > 1
          else (bad_files[0]['source'], bad_files[0]['extern']))
      long_text = self._UPDATE_MESSAGE % replacements
      results.append(self._output_api.PresubmitPromptWarning(
          str('Found updated extension api files without updated extern files. '
              'Please update the extern files.'),
          [f['source'] for f in bad_files],
          long_text))
    return results
