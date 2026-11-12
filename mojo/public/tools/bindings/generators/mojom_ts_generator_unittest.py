# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

from generators import mojom_ts_generator


class _MockModule:
  def __init__(self, path, webui_module_path=None, imported_kinds=None):
    self.path = path
    self.metadata = {}
    if webui_module_path is not None:
      self.metadata['webui_module_path'] = webui_module_path
    self.imported_kinds = imported_kinds or {}


class _MockKind:
  def __init__(self, name, module):
    self.name = name
    self.module = module


class MojomTsGeneratorTest(unittest.TestCase):
  def test_get_webui_module_path(self):
    self.assertIsNone(
      mojom_ts_generator._GetWebUiModulePath(_MockModule('a.mojom'))
    )
    self.assertEqual(
      mojom_ts_generator._GetWebUiModulePath(
        _MockModule('a.mojom', webui_module_path='')
      ),
      '/',
    )
    self.assertEqual(
      mojom_ts_generator._GetWebUiModulePath(
        _MockModule('a.mojom', webui_module_path='/')
      ),
      '/',
    )
    self.assertEqual(
      mojom_ts_generator._GetWebUiModulePath(
        _MockModule('a.mojom', webui_module_path='sub/dir')
      ),
      '/sub/dir/',
    )
    self.assertEqual(
      mojom_ts_generator._GetWebUiModulePath(
        _MockModule('a.mojom', webui_module_path='/sub/dir/')
      ),
      '/sub/dir/',
    )
    self.assertEqual(
      mojom_ts_generator._GetWebUiModulePath(
        _MockModule(
          'a.mojom', webui_module_path='chrome://resources/mojo/url/mojom'
        )
      ),
      'chrome://resources/mojo/url/mojom/',
    )

  def test_root_module_imports(self):
    shared_module = _MockModule(
      'mojo/public/mojom/base/time.mojom',
      webui_module_path='chrome://resources/mojo/mojo/public/mojom/base',
    )
    cr_components_module = _MockModule(
      'ui/webui/resources/cr_components/foo/component.mojom',
      webui_module_path='chrome://resources/cr_components/foo/',
    )
    root_dependency_module = _MockModule(
      'chrome/browser/foo/bar.mojom', webui_module_path='/'
    )
    subpath_dependency_module = _MockModule(
      'chrome/browser/foo/subpath.mojom', webui_module_path='/sub/dir'
    )
    time_kind = _MockKind('Time', shared_module)
    component_kind = _MockKind('Component', cr_components_module)
    bar_kind = _MockKind('Bar', root_dependency_module)
    subpath_kind = _MockKind('Subpath', subpath_dependency_module)

    module = _MockModule(
      'chrome/browser/ui/webui/baz/baz.mojom',
      webui_module_path='/',
      imported_kinds={
        'time': time_kind,
        'component': component_kind,
        'bar': bar_kind,
        'subpath': subpath_kind,
      },
    )
    generator = mojom_ts_generator.Generator(module)
    self.assertEqual(
      generator._GetJsModuleImports(),
      {
        '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js': [
          time_kind
        ],
        '//resources/cr_components/foo/component.mojom-webui.js': [
          component_kind
        ],
        './bar.mojom-webui.js': [bar_kind],
        './sub/dir/subpath.mojom-webui.js': [subpath_kind],
      },
    )

  def test_subpath_module_imports(self):
    shared_module = _MockModule(
      'mojo/public/mojom/base/time.mojom',
      webui_module_path='chrome://resources/mojo/mojo/public/mojom/base',
    )
    root_dependency_module = _MockModule(
      'chrome/browser/foo/bar.mojom', webui_module_path='/'
    )
    same_subpath_dependency_module = _MockModule(
      'chrome/browser/foo/subpath.mojom', webui_module_path='/sub/dir'
    )
    time_kind = _MockKind('Time', shared_module)
    bar_kind = _MockKind('Bar', root_dependency_module)
    subpath_kind = _MockKind('Subpath', same_subpath_dependency_module)

    module = _MockModule(
      'chrome/browser/ui/webui/baz/baz.mojom',
      webui_module_path='/sub/dir',
      imported_kinds={
        'time': time_kind,
        'bar': bar_kind,
        'subpath': subpath_kind,
      },
    )
    generator = mojom_ts_generator.Generator(module)
    self.assertEqual(
      generator._GetJsModuleImports(),
      {
        '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js': [
          time_kind
        ],
        '../../bar.mojom-webui.js': [bar_kind],
        './subpath.mojom-webui.js': [subpath_kind],
      },
    )

  def test_shared_module_imports_shared_module(self):
    base_module = _MockModule(
      'mojo/public/mojom/base/unguessable_token.mojom',
      webui_module_path='chrome://resources/mojo/mojo/public/mojom/base',
    )
    token_kind = _MockKind('UnguessableToken', base_module)

    url_module = _MockModule(
      'url/mojom/origin.mojom',
      webui_module_path='chrome://resources/mojo/url/mojom',
      imported_kinds={'token': token_kind},
    )
    generator = mojom_ts_generator.Generator(url_module)
    self.assertEqual(
      generator._GetJsModuleImports(),
      {
        '../../mojo/public/mojom/base/unguessable_token.mojom-webui.js': [
          token_kind
        ],
      },
    )

  def test_missing_dependency_webui_module_path_fails(self):
    dependency_without_webui = _MockModule('chrome/browser/foo/bar.mojom')
    bar_kind = _MockKind('Bar', dependency_without_webui)

    module = _MockModule(
      'chrome/browser/ui/webui/baz/baz.mojom',
      webui_module_path='/',
      imported_kinds={'bar': bar_kind},
    )
    generator = mojom_ts_generator.Generator(module)
    with self.assertRaises(AssertionError):
      generator._GetJsModuleImports()


if __name__ == '__main__':
  unittest.main()
