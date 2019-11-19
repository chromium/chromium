# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Functions shared by various parts of the code generator.

Design doc: http://www.chromium.org/developers/design-documents/idl-compiler
"""

import os
import re
import sys

from blinkbuild.name_style_converter import NameStyleConverter
from idl_types import IdlTypeBase
import idl_types
from idl_definitions import Exposure, IdlInterface, IdlAttribute
from utilities import to_snake_case
from v8_globals import includes

ACRONYMS = [
    'CSSOM',  # must come *before* CSS to match full acronym
    'CSS',
    'HTML',
    'IME',
    'JS',
    'SMIL',
    'SVG',
    'URL',
    'WOFF',
    'XML',
    'XSLT',
]


################################################################################
# Extended attribute parsing
################################################################################

def extended_attribute_value_contains(extended_attribute_value, key):
    return (extended_attribute_value == key or
            (isinstance(extended_attribute_value, list) and
             key in extended_attribute_value))


def has_extended_attribute(definition_or_member, extended_attribute_list):
    return any(extended_attribute in definition_or_member.extended_attributes
               for extended_attribute in extended_attribute_list)


def has_extended_attribute_value(definition_or_member, name, value):
    extended_attributes = definition_or_member.extended_attributes
    return (name in extended_attributes and
            extended_attribute_value_contains(extended_attributes[name], value))


def extended_attribute_value_as_list(definition_or_member, name):
    extended_attributes = definition_or_member.extended_attributes
    if name not in extended_attributes:
        return None
    value = extended_attributes[name]
    if isinstance(value, list):
        return value
    return [value]


################################################################################
# String handling
################################################################################

def capitalize(name):
    """Capitalize first letter or initial acronym (used in setter names)."""
    for acronym in ACRONYMS:
        if name.startswith(acronym.lower()):
            return name.replace(acronym.lower(), acronym)
    return name[0].upper() + name[1:]


def strip_suffix(string, suffix):
    if not suffix or not string.endswith(suffix):
        return string
    return string[:-len(suffix)]


def uncapitalize(name):
    """Uncapitalizes first letter or initial acronym (used in method names).

    E.g., 'SetURL' becomes 'setURL', but 'URLFoo' becomes 'urlFoo'.
    """
    for acronym in ACRONYMS:
        if name.startswith(acronym):
            return name.replace(acronym, acronym.lower())
    return name[0].lower() + name[1:]


def runtime_enabled_function(name):
    """Returns a function call of a runtime enabled feature."""
    return 'RuntimeEnabledFeatures::%sEnabled()' % name


################################################################################
# C++
################################################################################

def scoped_name(interface, definition, base_name):
    # partial interfaces are implemented as separate classes, with their members
    # implemented as static member functions
    partial_interface_implemented_as = definition.extended_attributes.get('PartialInterfaceImplementedAs')
    if partial_interface_implemented_as:
        return '%s::%s' % (partial_interface_implemented_as, base_name)
    if (definition.is_static or
            definition.name in ('Constructor', 'NamedConstructor')):
        return '%s::%s' % (cpp_name(interface), base_name)
    return 'impl->%s' % base_name


def v8_class_name(interface):
    return 'V8' + interface.name


def v8_class_name_or_partial(interface):
    class_name = v8_class_name(interface)
    if interface.is_partial:
        return ''.join([class_name, 'Partial'])
    return class_name


def build_basename(name, prefix=None):
    basename = to_snake_case(name)
    if prefix:
        basename = prefix + basename
    return basename


def binding_header_filename(name):
    """Returns a binding header filename for the specified interface name.

    E.g. 'NodeList' -> 'v8_node_list.h'
    """
    return build_basename(name, prefix='v8_') + '.h'


################################################################################
# Specific extended attributes
################################################################################

# [ActivityLogging]
def activity_logging_world_list(member, access_type=''):
    """Returns a set of world suffixes for which a definition member has activity logging, for specified access type.

    access_type can be 'Getter' or 'Setter' if only checking getting or setting.
    """
    extended_attributes = member.extended_attributes
    if 'LogActivity' not in extended_attributes:
        return set()
    log_activity = extended_attributes['LogActivity']
    if log_activity and not log_activity.startswith(access_type):
        return set()

    includes.add('platform/bindings/v8_dom_activity_logger.h')
    if 'LogAllWorlds' in extended_attributes:
        return set(['', 'ForMainWorld'])
    return set([''])  # At minimum, include isolated worlds.


# [ActivityLogging]
def activity_logging_world_check(member):
    """Returns if an isolated world check is required when generating activity
    logging code.

    The check is required when there is no per-world binding code and logging is
    required only for isolated world.
    """
    extended_attributes = member.extended_attributes
    if 'LogActivity' not in extended_attributes:
        return False
    if ('PerWorldBindings' not in extended_attributes and
            'LogAllWorlds' not in extended_attributes):
        return True
    return False


# [CallWith]
CALL_WITH_ARGUMENTS = {
    'Isolate': 'info.GetIsolate()',
    'ScriptState': 'script_state',
    'ExecutionContext': 'execution_context',
    'Document': 'document',
    'ThisValue': 'ScriptValue(info.GetIsolate(), info.Holder())',
}
# List because key order matters, as we want arguments in deterministic order
CALL_WITH_VALUES = [
    'Isolate',
    'ScriptState',
    'ExecutionContext',
    'Document',
    'ThisValue',
]


def call_with_arguments(call_with_values):
    if not call_with_values:
        return []
    return [CALL_WITH_ARGUMENTS[value]
            for value in CALL_WITH_VALUES
            if extended_attribute_value_contains(call_with_values, value)]


# [Constructor], [NamedConstructor]
def is_constructor_attribute(member):
    # TODO(yukishiino): replace this with [Constructor] and [NamedConstructor] extended attribute
    return (type(member) == IdlAttribute and
            member.idl_type.name.endswith('Constructor'))


# [DeprecateAs]
def deprecate_as(member):
    extended_attributes = member.extended_attributes
    if 'DeprecateAs' not in extended_attributes:
        return None
    includes.add('core/frame/deprecation.h')
    return extended_attributes['DeprecateAs']


# [Exposed]
EXPOSED_EXECUTION_CONTEXT_METHOD = {
    'AnimationWorklet': 'IsAnimationWorkletGlobalScope',
    'AudioWorklet': 'IsAudioWorkletGlobalScope',
    'DedicatedWorker': 'IsDedicatedWorkerGlobalScope',
    'LayoutWorklet': 'IsLayoutWorkletGlobalScope',
    'PaintWorklet': 'IsPaintWorkletGlobalScope',
    'ServiceWorker': 'IsServiceWorkerGlobalScope',
    'SharedWorker': 'IsSharedWorkerGlobalScope',
    'Window': 'IsDocument',
    'Worker': 'IsWorkerGlobalScope',
    'Worklet': 'IsWorkletGlobalScope',
}


EXPOSED_WORKERS = set([
    'DedicatedWorker',
    'SharedWorker',
    'ServiceWorker',
])


class ExposureSet:
    """An ExposureSet is a collection of Exposure instructions."""
    def __init__(self, exposures=None):
        self.exposures = set(exposures) if exposures else set()

    def issubset(self, other):
        """Returns true if |self|'s exposure set is a subset of
        |other|'s exposure set. This function doesn't care about
        RuntimeEnabled."""
        self_set = self._extended(set(e.exposed for e in self.exposures))
        other_set = self._extended(set(e.exposed for e in other.exposures))
        return self_set.issubset(other_set)

    @staticmethod
    def _extended(target):
        if EXPOSED_WORKERS.issubset(target):
            return target | set(['Worker'])
        elif 'Worker' in target:
            return target | EXPOSED_WORKERS
        return target

    def add(self, exposure):
        self.exposures.add(exposure)

    def __len__(self):
        return len(self.exposures)

    def __iter__(self):
        return self.exposures.__iter__()

    @staticmethod
    def _code(exposure):
        condition = ('execution_context->%s()' %
                   EXPOSED_EXECUTION_CONTEXT_METHOD[exposure.exposed])
        if exposure.runtime_enabled is not None:
            runtime_enabled = (runtime_enabled_function(exposure.runtime_enabled))
            return '({0} && {1})'.format(condition, runtime_enabled)
        return condition

    def code(self):
        if len(self.exposures) == 0:
            return None
        # We use sorted here to deflake output.
        return ' || '.join(sorted(self._code(e) for e in self.exposures))


def exposed(member, interface):
    """Returns a C++ code that checks if a method/attribute/etc is exposed.

    When the Exposed attribute contains RuntimeEnabledFeatures (i.e.
    Exposed(Arguments) form is given), the code contains check for them as
    well.

    EXAMPLE: [Exposed=Window, RuntimeEnabledFeature=Feature1]
      => context->isDocument()

    EXAMPLE: [Exposed(Window Feature1, Window Feature2)]
      => context->isDocument() && RuntimeEnabledFeatures::Feature1Enabled() ||
         context->isDocument() && RuntimeEnabledFeatures::Feature2Enabled()
    """
    exposure_set = ExposureSet(
        extended_attribute_value_as_list(member, 'Exposed'))
    interface_exposure_set = ExposureSet(
        extended_attribute_value_as_list(interface, 'Exposed'))
    for e in exposure_set:
        if e.exposed not in EXPOSED_EXECUTION_CONTEXT_METHOD:
            raise ValueError('Invalid execution context: %s' % e.exposed)

    # Methods must not be exposed to a broader scope than their interface.
    if not exposure_set.issubset(interface_exposure_set):
        raise ValueError('Interface members\' exposure sets must be a subset of the interface\'s.')

    return exposure_set.code()


# [SecureContext]
def secure_context(member, interface):
    """Returns C++ code that checks whether an interface/method/attribute/etc. is exposed
    to the current context. Requires that the surrounding code defines an |is_secure_context|
    variable prior to this check."""
    member_is_secure_context = 'SecureContext' in member.extended_attributes
    interface_is_secure_context = ((member.defined_in is None or
                                    member.defined_in == interface.name) and
                                   'SecureContext' in interface.extended_attributes)

    if not (member_is_secure_context or interface_is_secure_context):
        return None

    conditions = ['is_secure_context']

    if member_is_secure_context:
        conditional = member.extended_attributes['SecureContext']
        if conditional:
            conditions.append('!{}'.format(runtime_enabled_function(conditional)))

    if interface_is_secure_context:
        conditional = interface.extended_attributes['SecureContext']
        if conditional:
            conditions.append('!{}'.format(runtime_enabled_function(conditional)))

    return ' || '.join(conditions)


# [ImplementedAs]
def cpp_name(definition_or_member):
    extended_attributes = definition_or_member.extended_attributes
    if extended_attributes and 'ImplementedAs' in extended_attributes:
        return extended_attributes['ImplementedAs']
    # WebIDL identifiers can contain hyphens[1], but C++ identifiers cannot.
    # Therefore camelCase hyphen-containing identifiers.
    #
    # [1] https://heycam.github.io/webidl/#prod-identifier
    if '-' in definition_or_member.name:
        return NameStyleConverter(definition_or_member.name).to_lower_camel_case()
    return definition_or_member.name


def cpp_name_from_interfaces_info(name, interfaces_info):
    return interfaces_info.get(name, {}).get('implemented_as') or name


def cpp_name_or_partial(interface):
    cpp_class_name = cpp_name(interface)
    if interface.is_partial:
        return ''.join([cpp_class_name, 'Partial'])
    return cpp_class_name


# [MeasureAs]
def measure_as(definition_or_member, interface):
    extended_attributes = definition_or_member.extended_attributes
    if 'MeasureAs' in extended_attributes:
        includes.add('core/frame/web_feature.h')
        includes.add('platform/instrumentation/use_counter.h')
        return lambda suffix: extended_attributes['MeasureAs']
    if 'Measure' in extended_attributes:
        includes.add('core/frame/web_feature.h')
        includes.add('platform/instrumentation/use_counter.h')
        measure_as_name = capitalize(definition_or_member.name)
        if interface is not None:
            measure_as_name = '%s_%s' % (capitalize(interface.name), measure_as_name)
        return lambda suffix: 'V8%s_%s' % (measure_as_name, suffix)
    return None


# [HighEntropy]
def high_entropy(definition_or_member):
    extended_attributes = definition_or_member.extended_attributes
    if 'HighEntropy' in extended_attributes:
        includes.add('core/frame/dactyloscoper.h')
        if not ('Measure' in extended_attributes or 'MeasureAs' in extended_attributes):
            raise Exception('%s specified [HighEntropy], but does not include '
                            'either [Measure] or [MeasureAs]'
                            % definition_or_member.name)
        return True
    return False


# [RuntimeEnabled]
def _is_origin_trial_feature(feature_name, runtime_features):
    assert feature_name in runtime_features, feature_name + ' is not a runtime feature.'
    feature = runtime_features[feature_name]
    return feature['in_origin_trial']


def origin_trial_feature_name(definition_or_member, runtime_features):
    """
    Returns the name of the origin trial feature if found, None otherwise.
    Looks for origin trial feature specified by the RuntimeEnabled attribute.
    """
    extended_attributes = definition_or_member.extended_attributes
    feature_name = extended_attributes.get('RuntimeEnabled')
    if feature_name and _is_origin_trial_feature(feature_name, runtime_features):
        return feature_name


def origin_trial_function_call(feature_name, execution_context=None):
    """Returns a function call to determine if an origin trial is enabled."""
    return 'RuntimeEnabledFeatures::{feature_name}Enabled({context})'.format(
        feature_name=feature_name,
        context=execution_context if execution_context else "execution_context")


# [ContextEnabled]
def context_enabled_feature_name(definition_or_member):
    return definition_or_member.extended_attributes.get('ContextEnabled')


# [RuntimeCallStatsCounter]
def rcs_counter_name(member, generic_counter_name):
    extended_attribute_defined = 'RuntimeCallStatsCounter' in member.extended_attributes
    if extended_attribute_defined:
        counter = 'k' + member.extended_attributes['RuntimeCallStatsCounter']
    else:
        counter = generic_counter_name
    return (counter, extended_attribute_defined)


# [RuntimeEnabled]
def runtime_enabled_feature_name(definition_or_member, runtime_features):
    extended_attributes = definition_or_member.extended_attributes
    feature_name = extended_attributes.get('RuntimeEnabled')
    if feature_name and not _is_origin_trial_feature(feature_name, runtime_features):
        includes.add('platform/runtime_enabled_features.h')
        return feature_name


# [Unforgeable]
def is_unforgeable(member):
    return 'Unforgeable' in member.extended_attributes


# [Unforgeable], [Global]
def on_instance(interface, member):
    """Returns True if the interface's member needs to be defined on every
    instance object.

    The following members must be defined on an instance object.
    - [Unforgeable] members
    - regular members of [Global] interfaces
    """
    if member.is_static:
        return False

    # TODO(yukishiino): Remove a hack for ToString once we support
    # Symbol.ToStringTag.
    if interface.name == 'Window' and member.name == 'ToString':
        return False

    # TODO(yukishiino): Implement "interface object" and its [[Call]] method
    # in a better way.  Then we can get rid of this hack.
    if is_constructor_attribute(member):
        return True

    if ('Global' in interface.extended_attributes or
            'Unforgeable' in member.extended_attributes):
        return True
    return False


def on_prototype(interface, member):
    """Returns True if the interface's member needs to be defined on the
    prototype object.

    Most members are defined on the prototype object.  Exceptions are as
    follows.
    - static members (optional)
    - [Unforgeable] members
    - members of [Global] interfaces
    - named properties of [Global] interfaces
    """
    if member.is_static:
        return False

    # TODO(yukishiino): Remove a hack for toString once we support
    # Symbol.toStringTag.
    if (interface.name == 'Window' and member.name == 'toString'):
        return True

    # TODO(yukishiino): Implement "interface object" and its [[Call]] method
    # in a better way.  Then we can get rid of this hack.
    if is_constructor_attribute(member):
        return False

    if ('Global' in interface.extended_attributes or
            'Unforgeable' in member.extended_attributes):
        return False
    return True


# static, const
def on_interface(interface, member):
    """Returns True if the interface's member needs to be defined on the
    interface object.

    The following members must be defiend on an interface object.
    - static members
    """
    if member.is_static:
        return True
    return False


################################################################################
# Indexed properties
# http://heycam.github.io/webidl/#idl-indexed-properties
################################################################################

def indexed_property_getter(interface):
    try:
        # Find indexed property getter, if present; has form:
        # getter TYPE [OPTIONAL_IDENTIFIER](unsigned long ARG1)
        return next(
            method
            for method in interface.operations
            if ('getter' in method.specials and
                len(method.arguments) == 1 and
                str(method.arguments[0].idl_type) == 'unsigned long'))
    except StopIteration:
        return None


def indexed_property_setter(interface):
    try:
        # Find indexed property setter, if present; has form:
        # setter RETURN_TYPE [OPTIONAL_IDENTIFIER](unsigned long ARG1, ARG_TYPE ARG2)
        return next(
            method
            for method in interface.operations
            if ('setter' in method.specials and
                len(method.arguments) == 2 and
                str(method.arguments[0].idl_type) == 'unsigned long'))
    except StopIteration:
        return None


def indexed_property_deleter(interface):
    try:
        # Find indexed property deleter, if present; has form:
        # deleter TYPE [OPTIONAL_IDENTIFIER](unsigned long ARG)
        return next(
            method
            for method in interface.operations
            if ('deleter' in method.specials and
                len(method.arguments) == 1 and
                str(method.arguments[0].idl_type) == 'unsigned long'))
    except StopIteration:
        return None


################################################################################
# Named properties
# http://heycam.github.io/webidl/#idl-named-properties
################################################################################

def named_property_getter(interface):
    try:
        # Find named property getter, if present; has form:
        # getter TYPE [OPTIONAL_IDENTIFIER](DOMString ARG1)
        getter = next(
            method
            for method in interface.operations
            if ('getter' in method.specials and
                len(method.arguments) == 1 and
                str(method.arguments[0].idl_type) == 'DOMString'))
        getter.name = getter.name or 'AnonymousNamedGetter'
        return getter
    except StopIteration:
        return None


def named_property_setter(interface):
    try:
        # Find named property setter, if present; has form:
        # setter RETURN_TYPE [OPTIONAL_IDENTIFIER](DOMString ARG1, ARG_TYPE ARG2)
        return next(
            method
            for method in interface.operations
            if ('setter' in method.specials and
                len(method.arguments) == 2 and
                str(method.arguments[0].idl_type) == 'DOMString'))
    except StopIteration:
        return None


def named_property_deleter(interface):
    try:
        # Find named property deleter, if present; has form:
        # deleter TYPE [OPTIONAL_IDENTIFIER](DOMString ARG)
        return next(
            method
            for method in interface.operations
            if ('deleter' in method.specials and
                len(method.arguments) == 1 and
                str(method.arguments[0].idl_type) == 'DOMString'))
    except StopIteration:
        return None


IdlInterface.indexed_property_getter = property(indexed_property_getter)
IdlInterface.indexed_property_setter = property(indexed_property_setter)
IdlInterface.indexed_property_deleter = property(indexed_property_deleter)
IdlInterface.named_property_getter = property(named_property_getter)
IdlInterface.named_property_setter = property(named_property_setter)
IdlInterface.named_property_deleter = property(named_property_deleter)
