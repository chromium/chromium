#!/usr/bin/python
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script reads the global interface data collected by
# compute_interfaces_info_overall.py, and writes out the code which adds
# bindings for origin-trial-enabled features at runtime.

import optparse
import os
import posixpath
import sys
from collections import defaultdict, namedtuple

from code_generator import (initialize_jinja_env, normalize_and_sort_includes,
                            render_template)
from idl_reader import IdlReader
from utilities import (create_component_info_provider, write_file,
                       idl_filename_to_component)
from v8_utilities import (binding_header_filename, v8_class_name,
                          v8_class_name_or_partial, origin_trial_feature_name)

# Make sure extension is .py, not .pyc or .pyo, so doesn't depend on caching
MODULE_PYNAME = os.path.splitext(os.path.basename(__file__))[0] + '.py'


OriginTrialInterfaceInfo = namedtuple('OriginTrialInterfaceInfo', [
    'name', 'v8_class', 'v8_class_or_partial', 'is_global'])


def get_install_functions(interfaces, feature_names):
    """Construct a list of V8 bindings installation functions for each feature
    on each interface.

    interfaces is a list of OriginTrialInterfaceInfo tuples
    feature_names is a list of strings, containing names of features which can
        be installed on those interfaces.
    """
    return [
        {'condition': 'RuntimeEnabledFeatures::%sEnabled' % feature_name,
         'name': feature_name,
         'install_method': 'Install%s' % feature_name,
         'interface_is_global': interface_info.is_global,
         'v8_class': interface_info.v8_class,
         'v8_class_or_partial': interface_info.v8_class_or_partial}
        for feature_name in feature_names
        for interface_info in interfaces]


def get_origin_trial_feature_names_from_interface(interface, runtime_features):
    feature_names = set()

    def add_if_not_none(value):
        if value:
            feature_names.add(value)

    if interface.is_partial:
        add_if_not_none(origin_trial_feature_name(interface, runtime_features))
    for operation in interface.operations:
        add_if_not_none(origin_trial_feature_name(operation, runtime_features))
    for attribute in interface.attributes:
        add_if_not_none(origin_trial_feature_name(attribute, runtime_features))
    return feature_names


def read_idl_file(reader, idl_filename):
    definitions = reader.read_idl_file(idl_filename)
    interfaces = definitions.interfaces
    includes = definitions.includes
    # There should only be a single interface defined in an IDL file. Return it.
    assert len(interfaces) == 1
    return (interfaces.values()[0], includes)


def interface_is_global(interface):
    return 'Global' in interface.extended_attributes


def origin_trial_features_info(info_provider, reader, idl_filenames, target_component):
    """Read a set of IDL files and compile the mapping between interfaces and
    the conditional features defined on them.

    Returns a tuple (features_for_type, types_for_feature, includes):
      - features_for_type is a mapping of interface->feature
      - types_for_feature is the reverse mapping: feature->interface
      - includes is a set of header files which need to be included in the
          generated implementation code.
    """
    features_for_type = defaultdict(set)
    types_for_feature = defaultdict(set)
    include_files = set()
    runtime_features = info_provider.component_info['runtime_enabled_features']

    for idl_filename in idl_filenames:
        interface, includes = read_idl_file(reader, idl_filename)
        feature_names = get_origin_trial_feature_names_from_interface(interface, runtime_features)

        # If this interface is a mixin, we don't generate V8 bindings code for
        # it.
        if interface.is_mixin:
            continue

        # If this interface include another one,
        # it inherits any conditional features from it.
        for include in includes:
            assert include.interface == interface.name
            mixin, _ = read_idl_file(
                reader,
                info_provider.interfaces_info[include.mixin].get('full_path'))
            feature_names |= get_origin_trial_feature_names_from_interface(mixin, runtime_features)

        feature_names = list(feature_names)
        if feature_names:
            is_global = interface_is_global(interface)
            if interface.is_partial:
                # For partial interfaces, we need to generate different
                # |include_files| if the parent interface is in a different
                # component.
                parent_interface_info = info_provider.interfaces_info[interface.name]
                parent_interface, _ = read_idl_file(
                    reader, parent_interface_info.get('full_path'))
                is_global = is_global or interface_is_global(parent_interface)
                parent_component = idl_filename_to_component(
                    parent_interface_info.get('full_path'))
            if interface.is_partial and target_component != parent_component:
                include_files.add('bindings/%s/v8/%s' %
                                  (parent_component, binding_header_filename(interface.name)))
                include_files.add('bindings/%s/v8/%s' %
                                  (target_component, binding_header_filename(interface.name + 'Partial')))
            else:
                include_files.add('bindings/%s/v8/%s' %
                                  (target_component, binding_header_filename(interface.name)))
                # If this is a partial interface in the same component as
                # its parent, then treat it as a non-partial interface.
                interface.is_partial = False
            interface_info = OriginTrialInterfaceInfo(interface.name,
                                                      v8_class_name(interface),
                                                      v8_class_name_or_partial(
                                                          interface),
                                                      is_global)
            for feature_name in feature_names:
                features_for_type[interface_info].add(feature_name)
                types_for_feature[feature_name].add(interface_info)

    return features_for_type, types_for_feature, include_files


def origin_trial_features_context(generator_name, feature_info):
    context = {'code_generator': generator_name}

    # Unpack the feature info tuple.
    features_for_type, types_for_feature, include_files = feature_info

    # Add includes needed for cpp code and normalize.
    include_files.update([
        'core/context_features/context_feature_settings.h',
        'core/execution_context/execution_context.h',
        'core/frame/frame.h',
        'core/origin_trials/origin_trials.h',
        'platform/bindings/origin_trial_features.h',
        'platform/bindings/script_state.h',
        'platform/bindings/v8_per_context_data.h',
        'platform/runtime_enabled_features.h',
        # TODO(iclelland): Remove the need to explicitly include this; it is
        # here because the ContextFeatureSettings code needs it.
        'bindings/core/v8/v8_window.h',
    ])
    context['includes'] = normalize_and_sort_includes(include_files)

    # For each interface, collect a list of bindings installation functions to
    # call, organized by conditional feature.
    context['installers_by_interface'] = [
        {'name': interface_info.name,
         'is_global': interface_info.is_global,
         'v8_class': interface_info.v8_class,
         'installers': get_install_functions([interface_info], feature_names)}
        for interface_info, feature_names in features_for_type.items()]
    context['installers_by_interface'].sort(key=lambda x: x['name'])

    # For each conditional feature, collect a list of bindings installation
    # functions to call, organized by interface.
    context['installers_by_feature'] = [
        {'name': feature_name,
         'name_constant': 'OriginTrialFeature::k%s' % feature_name,
         'installers': get_install_functions(interfaces, [feature_name])}
        for feature_name, interfaces in types_for_feature.items()]
    context['installers_by_feature'].sort(key=lambda x: x['name'])

    return context


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('--cache-directory',
                      help='cache directory, defaults to output directory')
    parser.add_option('--output-directory')
    parser.add_option('--info-dir')
    parser.add_option('--target-component',
                      type='choice',
                      choices=['core', 'modules'],
                      help='target component to generate code')
    parser.add_option('--idl-files-list')

    options, _ = parser.parse_args()
    if options.output_directory is None:
        parser.error('Must specify output directory using --output-directory.')
    return options


def generate_origin_trial_features(info_provider, options, idl_filenames):
    reader = IdlReader(info_provider.interfaces_info, options.cache_directory)
    jinja_env = initialize_jinja_env(options.cache_directory)

    # Extract the bidirectional mapping of conditional features <-> interfaces
    # from the global info provider and the supplied list of IDL files.
    feature_info = origin_trial_features_info(info_provider,
                                              reader, idl_filenames,
                                              options.target_component)

    # Convert that mapping into the context required for the Jinja2 templates.
    template_context = origin_trial_features_context(
        MODULE_PYNAME, feature_info)

    file_basename = 'origin_trial_features_for_%s' % options.target_component

    # Generate and write out the header file
    header_text = render_template(jinja_env.get_template(file_basename + '.h.tmpl'), template_context)
    header_path = posixpath.join(options.output_directory, file_basename + '.h')
    write_file(header_text, header_path)

    # Generate and write out the implementation file
    cpp_text = render_template(jinja_env.get_template(file_basename + '.cc.tmpl'), template_context)
    cpp_path = posixpath.join(options.output_directory, file_basename + '.cc')
    write_file(cpp_text, cpp_path)


def main():
    options = parse_options()

    info_provider = create_component_info_provider(
        os.path.normpath(options.info_dir), options.target_component)
    idl_filenames = map(str.strip, open(options.idl_files_list))

    generate_origin_trial_features(info_provider, options, idl_filenames)
    return 0


if __name__ == '__main__':
    sys.exit(main())
