#!/usr/bin/env python3
#
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""MojoLPMGenerator

This script can be used to generate the necessary proto and c++ files in order
to fuzz a mojom interface using MojoLPM. Run this script with `--help` to see
how to use this script.

MojoLPMGenerator needs to be given a mojom interface (along with the file in
which it is defined). To correctly generate the necessary MojoLPM actions, it
walks the interface's dependency graph and gather all its interesting kinds,
such as `pending_remote`, `pending_receiver` or all the handle kinds. Note that
the tool also parses structures and unions to make sure the interface is
correctly fuzzed.

This tool assumes that the first interface provided is implemented by the
service being fuzzed (understand that MojoLPM should not emulate this
interface). For this reason, it generates a 'New' action, and the user needs
to provide a C++ method to create the appropriate remote.
It then walks the dependencies by taking into account whether the currently
parsed interface is emulated or not. This allows deducing what kinds of
MojoLPM actions need to be used. For instance, if the root interface being
parsed is `InterfaceA`, and this interface has a method that takes a
`pending_remote<InterfaceB>` as a parameter, the tool will generate a
`InterfaceB.ReceiverAction` for it. However, when recursively parsing
`InterfaceB` (which is emulated my MojoLPM in this example then), if the
interface has a method that takes a `pending_remote<InterfaceC>`, we will
generate a `InterfaceC.RemoteAction` for it. Indeed, this makes sense because
since `InterfaceB` is currently being emulated, MojoLPM will receive this
`pending_remote` as an argument, create a `mojo::Remote` for it, and add it to
its existing interfaces. Since the emulated interface is called by the service
being fuzzed, `InterfaceC` is likely implemented by the service as well (or one
of its components), and thus we want to make sure we make some calls to its
remote too.

Once all the MojoLPM actions are created, this tool will generate a `.proto` and
a `.h` file referencing those actions.
"""

from __future__ import annotations

import abc
import argparse
import dataclasses
import json
import os
import pathlib
import re
import sys

import typing
import enum

sys.path.insert(
    0,
    os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                 "mojom"))

from mojom import fileutil
from mojom.generate import module

fileutil.AddLocalRepoThirdPartyDirToModulePath()
CHROME_SRC_DIR = fileutil._GetDirAbove('mojo')

import jinja2


class MojoLPMDefinitionType(enum.Enum):
  """The definition type of the currently handled interface.
  """
  REMOTE = 1  # The interface is handled by the service.
  EMULATED = 2  # The interface is "emulated" by MojoLPM.


class MojomActionType(enum.Enum):
  """The mojom action type. This maps directly with the mojom python API.
  """
  ASSOCIATED_RECEIVER = 'rca'
  RECEIVER = 'rcv'
  ASSOCIATED_REMOTE = 'rma'
  REMOTE = 'rmt'


class MojomHandleType(enum.Enum):
  """Identifies the specific type of a mojo handle(e.g. `handle<message_pipe>`).
  """
  DATA_PIPE_CONSUMER = 1
  DATA_PIPE_PRODUCER = 2
  MESSAGE_PIPE = 3
  SHARED_BUFFER = 4
  PLATFORM = 5


class MojoLPMActionType(enum.Enum):
  """The MojoLPM action type. MojoLPM handles a limited set of actions, that
  we define in this enum.
  """
  NEW_ACTION = 'NewAction'
  REMOTE_ACTION = 'RemoteAction'
  RECEIVER_ACTION = 'ReceiverAction'
  ASSOCIATED_REMOTE_ACTION = 'AssociatedRemoteAction'
  DATA_PIPE_READ = 'DataPipeRead'
  DATA_PIPE_WRITE = 'DataPipeWrite'
  DATA_PIPE_CONSUMER_CLOSE = 'DataPipeConsumerClose'
  DATA_PIPE_PRODUCER_CLOSE = 'DataPipeProducerClose'
  SHARED_BUFFER_WRITE = 'SharedBufferWrite'
  SHARED_BUFFER_RELEASE = 'SharedBufferRelease'


@dataclasses.dataclass(frozen=True)
class MojoLPMAction:
  """Represents a MojoLPM action, like 'Interface.RemoteAction'. This also
  embeds a potential dependency on an interface file if needed.
  Members:
    type: the action type for this action. See MojoLPMActionType.
    namespace: the namespace that will prefix the action.
    identifier: the identifier of the action. This must be unique across all
    actions. The identifier should be a snake case string that will be used
    to generate field or message names.
    dependencies: the mojom file dependencies of this action.
  """
  type: MojoLPMActionType
  namespace: typing.Optional[str]
  identifier: str
  dependencies: typing.FrozenSet[str]

  @property
  def camel_case_namespace(self) -> typing.Optional[str]:
    if not self.namespace:
      return None
    ns = '_'.join(self.namespace.split('.')[:-1])
    return snake_to_camel_case(ns)

  @property
  def snake_case_namespace(self) -> typing.Optional[str]:
    if not self.namespace:
      return None
    ns = '_'.join(self.namespace.split('.')[:-1])
    return camel_to_snake_case(ns)

  @property
  def proto_identifier(self) -> str:
    """Returns the proto identifier for this action. The identifier will be
    used as a unique name for this action. For instance, it can either be a
    message name or a field name.
    """
    id_type = camel_to_snake_case(self.type.value)
    if self.snake_case_namespace:
      return f"{self.snake_case_namespace}_{self.identifier}_{id_type}"
    return f"{self.identifier}_{id_type}"

  @property
  def cpp_identifier(self) -> str:
    """Returns the cpp identifier for this action."""
    snake_id = snake_to_camel_case(self.identifier)
    if self.camel_case_namespace:
      return f"{self.camel_case_namespace}{snake_id}"
    return f"{snake_id}"

  @property
  def mojolpm_proto_type(self) -> str:
    """Returns the proto type for this action.

    Returns:
        the proto type as a string
    """
    if self.type == MojoLPMActionType.NEW_ACTION:
      assert self.camel_case_namespace
      ns = self.camel_case_namespace
      return f"{ns}{snake_to_camel_case(self.identifier)}"
    if not self.namespace:
      return f"mojolpm.{self.type.value}"
    return f"mojolpm.{self.namespace}.{self.type.value}"


_MOJOLPM_BASE_DEPENDENCY = "mojo/public/tools/fuzzers/mojolpm"
_DEFAULT_ACTION_DEPS = frozenset([_MOJOLPM_BASE_DEPENDENCY])

_EMULATED_HANDLE_ACTION_MAP = {
    MojomHandleType.DATA_PIPE_CONSUMER: [
        MojoLPMAction(
            type=MojoLPMActionType.DATA_PIPE_READ,
            namespace=None,
            identifier="data_pipe_read_action",
            dependencies=_DEFAULT_ACTION_DEPS,
        ),
        MojoLPMAction(
            type=MojoLPMActionType.DATA_PIPE_CONSUMER_CLOSE,
            namespace=None,
            identifier="data_pipe_consumer_close_action",
            dependencies=_DEFAULT_ACTION_DEPS,
        ),
    ],
    MojomHandleType.DATA_PIPE_PRODUCER: [
        MojoLPMAction(
            type=MojoLPMActionType.DATA_PIPE_WRITE,
            namespace=None,
            identifier="data_pipe_write_action",
            dependencies=_DEFAULT_ACTION_DEPS,
        ),
        MojoLPMAction(
            type=MojoLPMActionType.DATA_PIPE_PRODUCER_CLOSE,
            namespace=None,
            identifier="data_pipe_producer_close_action",
            dependencies=_DEFAULT_ACTION_DEPS,
        ),
    ],
    MojomHandleType.SHARED_BUFFER: [
        MojoLPMAction(
            type=MojoLPMActionType.SHARED_BUFFER_WRITE,
            namespace=None,
            identifier="shared_buffer_write",
            dependencies=_DEFAULT_ACTION_DEPS,
        ),
        MojoLPMAction(
            type=MojoLPMActionType.SHARED_BUFFER_RELEASE,
            namespace=None,
            identifier="shared_buffer_release",
            dependencies=_DEFAULT_ACTION_DEPS,
        ),
    ],
}

_REMOTE_HANDLE_ACTION_MAP = {
    MojomHandleType.DATA_PIPE_PRODUCER: [
        MojoLPMAction(
            type=MojoLPMActionType.DATA_PIPE_READ,
            namespace=None,
            identifier="data_pipe_read_action",
            dependencies=_DEFAULT_ACTION_DEPS,
        ),
        MojoLPMAction(
            type=MojoLPMActionType.DATA_PIPE_CONSUMER_CLOSE,
            namespace=None,
            identifier="data_pipe_consumer_close_action",
            dependencies=_DEFAULT_ACTION_DEPS,
        ),
    ],
    MojomHandleType.DATA_PIPE_CONSUMER: [
        MojoLPMAction(
            type=MojoLPMActionType.DATA_PIPE_WRITE,
            namespace=None,
            identifier="data_pipe_write_action",
            dependencies=_DEFAULT_ACTION_DEPS,
        ),
        MojoLPMAction(
            type=MojoLPMActionType.DATA_PIPE_PRODUCER_CLOSE,
            namespace=None,
            identifier="data_pipe_producer_close_action",
            dependencies=_DEFAULT_ACTION_DEPS,
        ),
    ],
    MojomHandleType.SHARED_BUFFER: [
        MojoLPMAction(
            type=MojoLPMActionType.SHARED_BUFFER_WRITE,
            namespace=None,
            identifier="shared_buffer_write",
            dependencies=_DEFAULT_ACTION_DEPS,
        ),
        MojoLPMAction(
            type=MojoLPMActionType.SHARED_BUFFER_RELEASE,
            namespace=None,
            identifier="shared_buffer_release",
            dependencies=_DEFAULT_ACTION_DEPS,
        ),
    ],
}


def _GetProtoId(name):
  # We reserve ids [0,15]
  # Protobuf implementation reserves [19000,19999]
  # Max proto id is 2^29-1
  # 32-bit fnv-1a
  fnv = 2166136261
  for c in name:
    fnv = fnv ^ ord(c)
    fnv = (fnv * 16777619) & 0xffffffff
  # xor-fold to 29-bits
  fnv = (fnv >> 29) ^ (fnv & 0x1fffffff)
  # now use a modulo to reduce to [0,2^29-1 - 1016]
  fnv = fnv % 536869895
  # now we move out the disallowed ranges
  fnv = fnv + 15
  if fnv >= 19000:
    fnv += 1000
  return fnv


def camel_to_snake_case(name: str) -> str:
  """Camel case to snake case conversion.

  Args:
      name: the camel case identifier

  Returns:
      `name` converted to a snake case identifier.
  """
  return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower()


def snake_to_camel_case(snake_str: str) -> str:
  """Snake case to camel case conversion.

  Args:
      snake_str: the snake case identifier to convert.

  Returns:
     `snake_str` converted to a camel case identifier.
  """
  return "".join(x.title() for x in snake_str.lower().split("_"))


def is_data_pipe_kind(kind: module.Kind) -> bool:
  """Returns whether kind is a data pipe kind, which are data_pipe_consumer and
  data_pipe_producer.
  """
  return module.IsDataPipeConsumerKind(kind) or module.IsDataPipeProducerKind(
      kind)


def is_pending_kind(kind: module.Kind) -> bool:
  """Returns whether kind is a pending kind.
  Exhaustive list: pending_remote, pending_receiver, pending_associated_remote,
  pending_associated_receiver.
  """
  return module.IsPendingRemoteKind(kind) or module.IsPendingReceiverKind(
      kind) or module.IsPendingAssociatedRemoteKind(
          kind) or module.IsPendingAssociatedReceiverKind(kind)


def is_interesting_kind(kind: module.Kind) -> bool:
  """Returns whether the kind is of interest for us. For instance, we are only
  interested in data_pipe kinds, pending kinds, struct kinds or union kinds.
  """
  return is_data_pipe_kind(kind) or is_pending_kind(
      kind) or module.IsStructKind(kind) or module.IsUnionKind(
          kind) or module.IsSharedBufferKind(kind)


def get_interesting_kind_deps(
    kind: module.ReferenceKind) -> typing.List[module.ReferenceKind]:
  """Returns the interesting kind deps found in the current kind. For instance,
  if the kind is an interface, this will iterate all its methods' parameters to
  search for interesting kinds. If the kind is a struct or a union, it will go
  through its fields.

  Args:
      kind: the kind.

  Returns:
      A list of interesting kind deps.
  """
  kinds = []
  if module.IsInterfaceKind(kind):
    params = []
    for m in kind.methods:
      params += m.parameters
    kinds = [p.kind for p in params]
  elif module.IsStructKind(kind) or module.IsUnionKind(kind):
    kinds = [f.kind for f in kind.fields]
  return filter(is_interesting_kind, kinds)


def format_dep_for_proto(dep: str) -> str:
  """Formats the given dependency as a proto dependency. This basically picks
  the `.proto` file for the given mojo interface.

  Args:
      dep: the dependency path.

  Returns:
      the proto formatted dependency path.
  """
  if dep == _MOJOLPM_BASE_DEPENDENCY:
    return f'{dep}.proto'
  return f'{dep}.mojolpm.proto'


def format_dep_for_cpp(dep: str) -> str:
  """Formats the given dependency as a C++ dependency. This basically picks
  the `-mojolpm.h` file for the given mojo interface.

  Args:
      dep: the dependency path.

  Returns:
      the C++ formatted dependency path.
  """
  if dep == _MOJOLPM_BASE_DEPENDENCY:
    return f'{dep}.h'
  return f'{dep}-mojolpm.h'


class MojoLPMActionSet:
  """A set of MojoLPMAction.
  Note that using lists instead of sets here is deliberate. It aims at
  preserving the order of actions in order to ease debugging.
  """

  def __init__(self,
               actions: typing.Optional[typing.List[MojoLPMAction]] = None):
    self.actions: typing.List[MojoLPMAction] = []
    self.deps: typing.List[str] = []
    if actions:
      for action in actions:
        self.add_action(action)

  def update(self, action_set: MojoLPMActionSet):
    """Adds all actions from `action_set` to this instance.

    Same semantics as `dict.update()`.

    Args:
        action_set: Another MojoLPMActionSet.
    """
    self.add_actions(action_set.actions)

  def add_action(self, action: MojoLPMAction):
    """Adds an action to the set.

    Args:
        action: the action.
    """
    if action not in self.actions:
      self.actions.append(action)

    for dep in action.dependencies:
      if not dep in self.deps:
        self.deps.append(dep)

  def add_actions(self, actions: typing.List[MojoLPMAction]):
    """Adds actions to the set.

    Args:
        actions: the list of actions.
    """
    for action in actions:
      self.add_action(action)


class MojoLPMGenerator(abc.ABC):
  """This class is the base class for representing a generator. A generator
  helps rendering a MojoLPMActionSet.
  """

  @abc.abstractmethod
  def render(self, action_list: typing.List[MojoLPMActionSet]):
    """Renders the given actions.
    """


class MojoLPMJinjaGenerator(MojoLPMGenerator):
  """Abstract class to help create a Jinja based generator.
  """

  def __init__(self, filepath: pathlib.PurePosixPath, template_filename: str):
    """Inits the generator.

    Args:
        filepath: the file path where the file should be generated.
        template_filename: the filename of the template.
    """
    self.filepath: pathlib.PurePosixPath = filepath
    self._environment = jinja2.Environment(loader=jinja2.FileSystemLoader(
        os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     "mojolpm_generator_templates/")))
    self._environment.globals['proto_id'] = _GetProtoId
    self.template = self._environment.get_template(template_filename)


class MojoLPMProtoGenerator(MojoLPMJinjaGenerator):
  """MojoLPMJinjaGenerator that renders a proto file with the given actions and
  dependencies. It uses jinja2 with a template file under the hood.
  """

  def __init__(self, filepath: pathlib.PurePosixPath, ensure_remote: bool):
    super().__init__(filepath, "mojolpm_generator.proto.tmpl")
    self._ensure_remote = ensure_remote

  def render(self, action_list: typing.List[MojoLPMActionSet]):
    all_actions_set = MojoLPMActionSet()
    for action_set in action_list:
      all_actions_set.update(action_set)
    new_messages = [
        a.mojolpm_proto_type for a in all_actions_set.actions
        if a.type == MojoLPMActionType.NEW_ACTION
    ]
    actions_list = [[{
        "proto_type": a.mojolpm_proto_type,
        "proto_identifier": a.proto_identifier,
        "is_new_action": a.type == MojoLPMActionType.NEW_ACTION,
    } for a in action_set.actions] for action_set in action_list]
    context = {
        "imports": [format_dep_for_proto(t) for t in all_actions_set.deps],
        "new_messages": new_messages,
        "actions_list": actions_list,
        "basename": self.filepath.name,
        "ensure_remote": self._ensure_remote,
    }
    proto_file = self.filepath.with_suffix('.proto')
    with pathlib.Path(proto_file).open(mode="w") as f:
      f.write(self.template.render(context))


class MojoLPMCppGenerator(MojoLPMJinjaGenerator):
  """MojoLPMJinjaGenerator that renders a C++ file with the given actions and
  dependencies. It uses jinja2 with a template file under the hood.
  """

  def __init__(self, filepath: pathlib.PurePosixPath, ensure_remote: bool):
    super().__init__(filepath, "mojolpm_generator.h.tmpl")
    self._ensure_remote = ensure_remote

  def render(self, action_list: typing.List[MojoLPMActionSet]):
    all_actions_set = MojoLPMActionSet()
    for action_set in action_list:
      all_actions_set.update(action_set)

    actions_list = []
    for action_set in action_list:
      actions = []
      for a in action_set.actions:
        if a.type == MojoLPMActionType.NEW_ACTION:
          actions.append({
              "case_name":
              "k" + snake_to_camel_case(a.proto_identifier),
              "cpp_name":
              a.cpp_identifier,
              "mojo_name":
              a.proto_identifier,
              "is_new_action":
              True,
          })
        else:
          actions.append({
              "case_name":
              "k" + snake_to_camel_case(a.proto_identifier),
              "mojolpm_func":
              "mojolpm::Handle" + a.type.value,
              "mojo_name":
              a.proto_identifier,
              "is_new_action":
              False,
          })
      actions_list.append(actions)
    if self.filepath.parts[0] == 'gen':
      rebased_path = self.filepath.relative_to('gen')
    else:
      rebased_path = self.filepath
    context = {
        'imports': [format_dep_for_cpp(t) for t in all_actions_set.deps],
        "actions_list": actions_list,
        "filename": rebased_path.with_suffix('.h').as_posix(),
        "proto_filename": rebased_path.with_suffix('.pb.h').as_posix(),
        "basename": snake_to_camel_case(self.filepath.name),
        "proto_namespace": f'mojolpmgenerator::{self.filepath.name}',
        "ensure_remote": self._ensure_remote,
    }
    with pathlib.Path(self.filepath.with_suffix('.h')).open(mode='w') as f:
      f.write(self.template.render(context))


class MojoLPMGeneratorMultiplexer(MojoLPMGenerator):

  def __init__(self, generators: typing.List[MojoLPMGenerator]):
    self._generators = generators

  def render(self, action_list: typing.List[MojoLPMActionSet]):
    for generator in self._generators:
      generator.render(action_list)


def build_handle_actions(handle_type: MojomHandleType,
                         def_type: MojoLPMDefinitionType) -> MojoLPMActionSet:
  """Builds the handle actions.

  Args:
      handle_type: the handle type.
      def_type: the current interface definition type.

  Returns:
      the set of actions to be generated.
  """
  # Not meaningful in the context of mojolpm
  if handle_type in (
      MojomHandleType.MESSAGE_PIPE,
      MojomHandleType.PLATFORM,
  ):
    return MojoLPMActionSet()
  return MojoLPMActionSet((_REMOTE_HANDLE_ACTION_MAP[handle_type]
                           if def_type == MojoLPMDefinitionType.REMOTE else
                           _EMULATED_HANDLE_ACTION_MAP[handle_type]))


def build_new_actions(interface: module.Interface) -> MojoLPMActionSet:
  """Builds a 'New' action for the given interface.

  Args:
      interface: the interface for which to generate a 'New' action.

  Returns:
      the set of actions to be generated.
  """
  return MojoLPMActionSet([
      MojoLPMAction(
          type=MojoLPMActionType.NEW_ACTION,
          namespace=interface.qualified_name,
          identifier=camel_to_snake_case(interface.mojom_name),
          dependencies=frozenset(),
      )
  ])


def build_pending_actions(
    action_type: MojomActionType,
    interface: module.Interface,
    def_type: MojoLPMDefinitionType,
) -> MojoLPMActionSet:
  """Builds a 'Pending' action for the given interface and action type.

  Args:
      action_type: the current action type to be generated.
      interface: the current interface for which to generate the action.
      def_type: the current interface definition type.

  Returns:
      the set of actions to be generated.
  """
  remote_interface = {
      MojomActionType.ASSOCIATED_RECEIVER:
      MojoLPMActionType.ASSOCIATED_REMOTE_ACTION,
      MojomActionType.RECEIVER: MojoLPMActionType.REMOTE_ACTION,
      MojomActionType.ASSOCIATED_REMOTE: MojoLPMActionType.RECEIVER_ACTION,
      MojomActionType.REMOTE: MojoLPMActionType.RECEIVER_ACTION,
  }
  emulated_interface = {
      MojomActionType.ASSOCIATED_RECEIVER: MojoLPMActionType.RECEIVER_ACTION,
      MojomActionType.RECEIVER: MojoLPMActionType.RECEIVER_ACTION,
      MojomActionType.ASSOCIATED_REMOTE:
      MojoLPMActionType.ASSOCIATED_REMOTE_ACTION,
      MojomActionType.REMOTE: MojoLPMActionType.REMOTE_ACTION,
  }
  return MojoLPMActionSet([
      MojoLPMAction(
          type=remote_interface[action_type] if def_type
          == MojoLPMDefinitionType.REMOTE else emulated_interface[action_type],
          namespace=interface.qualified_name,
          identifier=camel_to_snake_case(interface.mojom_name),
          dependencies=frozenset([f"{interface.module.path}"]),
      )
  ])


def build(interface: module.Interface,
          remote_type: MojoLPMActionType) -> MojoLPMActionSet:
  """Recursively builds the actions for the given interface.

  Args:
      interface: the interface for which to generate the action.
      remote_type: the kind of remote being initially passed in

  Returns:
      the set of actions to be generated.
  """
  handled_definitions = set()
  actions = MojoLPMActionSet()

  def __build_impl(current: module.ReferenceKind,
                   def_type: MojoLPMDefinitionType):
    kinds = get_interesting_kind_deps(current)
    for kind in kinds:
      if is_data_pipe_kind(kind):
        if module.IsDataPipeProducerKind(kind):
          handle_type = MojomHandleType.DATA_PIPE_PRODUCER
        else:
          handle_type = MojomHandleType.DATA_PIPE_CONSUMER
        actions.update(build_handle_actions(handle_type, def_type))
        continue
      if module.IsSharedBufferKind(kind):
        actions.update(
            build_handle_actions(MojomHandleType.SHARED_BUFFER, def_type))
        continue

      child_def_type = def_type
      if is_pending_kind(kind):
        # MojoLPM doesn't generate actions when the interface has no methods.
        if kind.kind.methods:
          # kind.spec will look something like '{?}rcv:x:blink.mojom.Blob'. This
          # is meant at fully describing the underlying type being parsed. For
          # instance, we only care about knowing the mojom action type here,
          # which is 'rcv' in this particular example.
          t = kind.spec.split(':')[0].lstrip('?')
          actions.update(
              build_pending_actions(MojomActionType(t), kind.kind, def_type))

          if module.IsPendingRemoteKind(
              kind) or module.IsPendingAssociatedRemoteKind(kind):
            child_def_type = (MojoLPMDefinitionType.EMULATED
                              if def_type == MojoLPMDefinitionType.REMOTE else
                              MojoLPMDefinitionType.REMOTE)
        kind = kind.kind

      assert module.IsInterfaceKind(kind) or module.IsStructKind(
          kind) or module.IsUnionKind(kind)
      if kind.qualified_name not in handled_definitions:
        handled_definitions.add(kind.qualified_name)
        __build_impl(kind, child_def_type)

  actions.update(build_new_actions(interface))
  # We want to add a mojolpm RemoteAction to our base interface.
  # Since the interface is remote, we'll add an action as if it was
  # declared in the mojo interface as is:
  # Create(pending_receiver<BaseInterface>);
  if interface.methods:
    if remote_type == MojoLPMActionType.ASSOCIATED_REMOTE_ACTION:
      action_type = MojomActionType.ASSOCIATED_RECEIVER
    else:
      action_type = MojomActionType.RECEIVER
    actions.update(
        build_pending_actions(action_type, interface,
                              MojoLPMDefinitionType.REMOTE))

  handled_definitions.add(interface.qualified_name)
  __build_impl(interface, MojoLPMDefinitionType.REMOTE)
  return actions


def get_interface_list_from_file(
    file_path: str) -> typing.List[typing.List[str]]:
  """Reads the JSON input file and returns the interfaces list that it
  contains.

  Args:
      file_path: the path to the input file.

  Returns:
      the list of interfaces.
  """
  with open(file_path, 'r') as f:
    data = json.load(f)
    return data['interfaces']


def get_interface_list_from_input(
    interfaces: typing.List[str]) -> typing.List[typing.List[str]]:
  """Parses the input list of interfaces and returns a list of list that
  matches the expected format.

  Args:
      interfaces: the list of strings listing the interfaces.

  Returns:
      the list of interfaces.
  """
  return [interface.split(':') for interface in interfaces]


def main():
  parser = argparse.ArgumentParser(
      description='Generate MojoLPM proto and cpp/h files.')
  group = parser.add_mutually_exclusive_group(required=True)
  group.add_argument(
      '-i',
      '--input',
      default=[],
      nargs='+',
      help="input(s) with format: "
      "path/to/interface.mojom-module:InterfaceName:{Remote|AssociatedRemote}")
  group.add_argument('-f', '--file', help="")
  parser.add_argument('--output_file_format',
                      required=True,
                      help="output file format. Files with extensions '.h' and"
                      " '.proto' will be created.")
  parser.add_argument(
      '-e',
      '--ensure-remote',
      action='store_true',
      default=False,
      help="For every listed remotes, ensure the 'new' action is called before"
      " any other actions related to the remote.")

  args = parser.parse_args()
  output_file = pathlib.PurePosixPath(args.output_file_format)

  generator = MojoLPMGeneratorMultiplexer([
      MojoLPMProtoGenerator(output_file, args.ensure_remote),
      MojoLPMCppGenerator(output_file, args.ensure_remote)
  ])
  actions: typing.List[MojoLPMActionSet] = []
  if args.file:
    interfaces = get_interface_list_from_file(args.file)
  else:
    interfaces = get_interface_list_from_input(args.input)
  for custom_format in interfaces:
    (file, interface_name, remote_type_str) = custom_format
    if remote_type_str == 'Remote':
      remote_type = MojoLPMActionType.REMOTE_ACTION
    else:
      remote_type = MojoLPMActionType.ASSOCIATED_REMOTE_ACTION
    with open(file, 'rb') as f:
      m = module.Module.Load(f)
      for interface in m.interfaces:
        if interface_name in (interface.mojom_name, interface.qualified_name):
          actions.append(build(interface, remote_type))
          break
  generator.render(actions)


if __name__ == "__main__":
  main()
