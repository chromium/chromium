# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates C++ source files from a mojom.Module with a WebUIJsBridge"""
import os.path
import sys


# Set up |sys.path| so that this module works without user-side setup of
# PYTHONPATH assuming Chromium's directory tree structure.
def _setup_sys_path():
  expected_path = 'mojo/public/tools/bindings/generators/'

  this_dir = os.path.dirname(__file__)
  root_dir = os.path.abspath(
      os.path.join(this_dir, *(['..'] * expected_path.count('/'))))

  module_dirs = (
      # //mojo/public/tools/mojom
      os.path.join(root_dir, "mojo", "public", "tools", "mojom"), )
  for module_dir in reversed(module_dirs):
    # Preserve sys.path[0] as is.
    # https://docs.python.org/3/library/sys.html?highlight=path[0]#sys.path
    sys.path.insert(1, module_dir)


_setup_sys_path()

import argparse
from mojom.generate import generator
from mojom.generate.template_expander import UseJinja
from mojom.generate import template_expander
# Import it as "mojom" because "module" is a common variable name.
import mojom.generate.module as mojom

GENERATOR_PREFIX = 'webui_js_bridge'


class WebUIJsBridgeImplStylizer(generator.Stylizer):
  def StylizeConstant(self, mojom_name):
    return mojom_name

  def StylizeField(self, mojom_name):
    return mojom_name

  def StylizeStruct(self, mojom_name):
    return mojom_name

  def StylizeUnion(self, mojom_name):
    return mojom_name

  def StylizeParameter(self, mojom_name):
    return mojom_name

  def StylizeMethod(self, mojom_name):
    return mojom_name

  def StylizeEnumField(self, mojom_name):
    return mojom_name

  def StylizeEnum(self, mojom_name):
    return mojom_name

  def StylizeModule(self, mojom_namespace):
    return '::'.join(mojom_namespace.split('.'))


class Generator(generator.Generator):
  def __init__(self, *args, **kwargs):
    super(Generator, self).__init__(*args, **kwargs)

  @staticmethod
  def GetTemplatePrefix():
    return 'webui_js_bridge_templates'

  def _GetKindName(self, kind):
    """Return `kind`'s name, prefixing it with its namespace if necessary."""
    should_include_namespace = self.module.path != kind.module.path
    if should_include_namespace:
      return f'{kind.module.namespace}::{kind.name}'
    return kind.name

  def _GetCppType(self, kind):
    """Return a string representing the C++ type of `kind`

     Returns None if the type is not supposed to be used in a WebUIJsBridge.
     """
    if mojom.IsPendingRemoteKind(kind):
      return f'::mojo::PendingRemote<{self._GetKindName(kind.kind)}>'
    if mojom.IsPendingReceiverKind(kind):
      return f'::mojo::PendingReceiver<{self._GetKindName(kind.kind)}>'

  def _GetInterfaceName(self, kind):
    """Return a string representing the C++ type of an endpoint's interface."""
    if not (mojom.IsPendingRemoteKind(kind)
            or mojom.IsPendingReceiverKind(kind)):
      raise Exception('Kind is neither a PendingRemote or PendingReceiver.')

    return self._GetKindName(kind.kind)

  def _IsReceiverAndRemoteBinder(self, bind_method):
    if len(bind_method.parameters) == 1:
      return False

    assert mojom.IsPendingReceiverKind(bind_method.parameters[0].kind)
    assert mojom.IsPendingRemoteKind(bind_method.parameters[1].kind)
    return True

  def _IsReceiverOnlyBinder(self, bind_method):
    if len(bind_method.parameters) == 2:
      return False

    return mojom.IsPendingReceiverKind(bind_method.parameters[0].kind)

  def _GetBinderMemberVariableName(self, bind_method):
    """Return the variable name of the binder corresponding to `bind_method`."""
    return f'binder_for_{generator.ToLowerSnakeCase(bind_method.name)}'

  def GetFilters(self):
    return {
        'cpp_type': self._GetCppType,
        'interface_name': self._GetInterfaceName,
        'binder_variable_name': self._GetBinderMemberVariableName,
        'is_receiver_only_binder': self._IsReceiverOnlyBinder,
        'is_receiver_and_remote_binder': self._IsReceiverAndRemoteBinder,
    }

  def _GetValidatedWebUIJsBridge(self):
    """Returns an Interface object for the WebUIJsBridge in this module.

    Raises an Exception if the interface doesn't satisfy WebUIJsBridge
    constraints.
    """
    webui_js_bridges = [
        interface for interface in self.module.interfaces
        if (interface.attributes and interface.attributes.get('WebUIJsBridge'))
    ]
    if len(webui_js_bridges) > 1:
      raise Exception('Found more than one WebUIJsBridge in '
                      f'{self.module.path}. Only one WebUIJsBridge is '
                      'supported per mojom target.')
    if len(webui_js_bridges) == 0:
      raise Exception(f'Found no WebUIJsBridge in {self.module.path}.')

    # Enforce WebUIJsBridges constraints.
    webui_js_bridge = webui_js_bridges[0]
    for method in webui_js_bridge.methods:
      if method.response_parameters != None:
        raise Exception(f'{webui_js_bridge.name}.{method.name} has a '
                        'response. WebUIJsBridge\'s methods should not '
                        'have responses.')

      if len(method.parameters) == 1:
        first_param = method.parameters[0]
        if not (mojom.IsPendingReceiverKind(first_param.kind)
                or mojom.IsPendingRemoteKind(first_param.kind)):
          raise Exception(f'{webui_js_bridge.name}.{method.name}\'s first '
                          'parameter should be a pending_receiver or a '
                          'pending_remote.')
      elif len(method.parameters) == 2:
        if not mojom.IsPendingRemoteKind(method.parameters[1].kind):
          raise Exception(f'{webui_js_bridge.name}.{method.name}\'s second '
                          'parameter can only be a pending_remote.')
      else:
        raise Exception(f'{webui_js_bridge.name}.{method.name} has '
                        f'{len(method.parameters)} parameters, but should have '
                        'one or two parameters: (1) A pending_receiver, (2) a '
                        'pending_remote, or (3) a pending_receiver and a '
                        'pending_remote.')

    return webui_js_bridge

  def _GetParameters(self):
    webui_controller_namespace = None
    webui_controller_name = None

    namespace_end = self.webui_controller_with_namespace.rfind("::")
    if namespace_end == -1:
      webui_controller_name = self.webui_controller_with_namespace
    else:
      webui_controller_namespace = \
        self.webui_controller_with_namespace[:namespace_end]
      webui_controller_name = \
        self.webui_controller_with_namespace[namespace_end + 2:]

    return {
        'module': self.module,
        'webui_js_bridge': self._GetValidatedWebUIJsBridge(),
        'webui_controller_name': webui_controller_name,
        'webui_controller_namespace': webui_controller_namespace,
        'webui_controller_header': self.webui_controller_header,
        'webui_controller_with_namespace': self.webui_controller_with_namespace,
    }

  @UseJinja('webui_js_bridge_impl.h.tmpl')
  def _GenerateWebUIJsBridgeImplDeclaration(self):
    return self._GetParameters()

  @UseJinja('webui_js_bridge_impl.cc.tmpl')
  def _GenerateWebUIJsBridgeImplDefinition(self):
    return self._GetParameters()

  def GenerateFiles(self, unparsed_args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--webui_js_bridge_config', dest='config')
    args = parser.parse_args(unparsed_args)
    (self.webui_controller_with_namespace, self.webui_controller_header) = \
        args.config.split('=')

    self.module.Stylize(WebUIJsBridgeImplStylizer())
    self.WriteWithComment(self._GenerateWebUIJsBridgeImplDeclaration(),
                          f'{self.module.path}-webui-js-bridge-impl.h')
    self.WriteWithComment(self._GenerateWebUIJsBridgeImplDefinition(),
                          f'{self.module.path}-webui-js-bridge-impl.cc')
