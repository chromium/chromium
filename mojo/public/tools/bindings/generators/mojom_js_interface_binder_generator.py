# Copyright 2023 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates C++ source files from a mojom.Module with a JSInterfaceBinder"""
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

GENERATOR_PREFIX = 'js_interface_binder'


class JsInterfaceBinderImplStylizer(generator.Stylizer):
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
    return 'js_interface_binder_templates'

  def _GetKindName(self, kind):
    """Return `kind`'s name, prefixing it with its namespace if necessary."""
    should_include_namespace = self.module.path != kind.module.path
    if should_include_namespace:
      return f'{kind.module.namespace}::{kind.name}'
    return kind.name

  def _GetCppType(self, kind):
    """Return a string representing the C++ type of `kind`

     Returns None if the type is not supposed to be used in a JsInterfaceBinder.
     """
    if mojom.IsPendingRemoteKind(kind):
      return f'::mojo::PendingRemote<{self._GetKindName(kind.kind)}>'
    if mojom.IsPendingReceiverKind(kind):
      return f'::mojo::PendingReceiver<{self._GetKindName(kind.kind)}>'

  def _GetBinderMemberVariableName(self, bind_method):
    """Return the variable name of the binder corresponding to `bind_method`."""
    return f'binder_for_{generator.ToLowerSnakeCase(bind_method.name)}'

  def GetFilters(self):
    return {
        'cpp_type': self._GetCppType,
        'binder_variable_name': self._GetBinderMemberVariableName,
    }

  def _GetValidatedInterfaceBinder(self):
    """Returns an Interface object for the JsInterfaceBinder..

    Raises an Exception if the interface doesn't satisfy JsInterfaceBinder
    constraints.
    """
    interface_binders = [
        interface for interface in self.module.interfaces if
        (interface.attributes and interface.attributes.get('JsInterfaceBinder'))
    ]
    if len(interface_binders) > 1:
      raise Exception('Found more than one JsInterfaceBinder in '
                      f'{self.module.path}. Only one JsInterfaceBinder is '
                      'supported per mojom target.')
    if len(interface_binders) == 0:
      raise Exception(f'Found no JsInterfaceBinder in {self.module.path}.')

    # Enforce JsInterfaceBinders constraints.
    interface_binder = interface_binders[0]
    for method in interface_binder.methods:
      if method.response_parameters != None:
        raise Exception(f'{interface_binder.name}.{method.name} has a '
                        'response. JsInterfaceBinder\'s methods should not '
                        'have responses.')

      for param in method.parameters:
        if not (mojom.IsPendingReceiverKind(param.kind)
                or mojom.IsPendingRemoteKind(param.kind)):
          raise Exception(f'{interface_binder.name}.{method.name}\'s '
                          f'"{param.name}" is not a pending_receiver or a '
                          'pending_remote. JSInterfaceBinder\'s methods '
                          'should only have pending_receiver or '
                          'pending_remote parameters.')

    return interface_binder

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
        'interface_binder': self._GetValidatedInterfaceBinder(),
        'webui_controller_name': webui_controller_name,
        'webui_controller_namespace': webui_controller_namespace,
        'webui_controller_header': self.webui_controller_header,
    }

  @UseJinja('js_interface_binder_impl.h.tmpl')
  def _GenerateJsInterfaceBinderImplDeclaration(self):
    return self._GetParameters()

  @UseJinja('js_interface_binder_impl.cc.tmpl')
  def _GenerateJsInterfaceBinderImplDefinition(self):
    return self._GetParameters()

  def GenerateFiles(self, unparsed_args):
    parser = argparse.ArgumentParser()
    parser.add_argument('--js_interface_binder_config', dest='config')
    args = parser.parse_args(unparsed_args)
    (self.webui_controller_with_namespace, self.webui_controller_header) = \
        args.config.split('=')

    self.module.Stylize(JsInterfaceBinderImplStylizer())
    self.WriteWithComment(self._GenerateJsInterfaceBinderImplDeclaration(),
                          f'{self.module.path}-js-interface-binder-impl.h')
    self.WriteWithComment(self._GenerateJsInterfaceBinderImplDefinition(),
                          f'{self.module.path}-js-interface-binder-impl.cc')
