# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import make_runtime_features_utilities as util
import json5_generator
import template_expander


class FeaturePolicyFeatureWriter(json5_generator.Writer):
    file_basename = 'feature_policy_helper'

    def __init__(self, json5_file_path, output_dir):
        super(FeaturePolicyFeatureWriter, self).__init__(json5_file_path, output_dir)
        self._outputs = {
            (self.file_basename + '.cc'): self.generate_implementation,
        }

        self._features = self.json5_file.name_dictionaries
        # Set runtime and feature policy features
        self._runtime_features = []
        self._feature_policy_features = []
        for feature in self._features:
            if feature['feature_policy_name']:
                self._feature_policy_features.append(feature)
            else:
                self._runtime_features.append(feature)

        self._convert_runtime_name_to_feature()
        self._set_in_origin_trial()
        self._runtime_to_feature_policy_map = self._make_runtime_to_feature_policy_map()
        self._origin_trial_dependency_map = self._make_origin_trial_dependency_map()
        self._header_guard = self.make_header_guard(self._relative_output_dir + self.file_basename + '.h')

    # Replaces runtime names in 'depends_on' list to feature objects.
    def _convert_runtime_name_to_feature(self):
        name_to_runtime_feature_map = {}
        for feature in self._runtime_features:
            name_to_runtime_feature_map[str(feature['name'])] = feature

        def replace_name_with_object(name):
            assert name in name_to_runtime_feature_map, name + ' is not a runtime feature.'
            return name_to_runtime_feature_map[name]

        for feature in self._feature_policy_features:
            feature['depends_on'] = map(replace_name_with_object, feature['depends_on'])

    # Returns a map of runtime features (not in origin trial) to the feature
    # policy features that depend on them. The maps is used to generate
    # DefaultFeatureNameMap().
    def _make_runtime_to_feature_policy_map(self):
        runtime_to_feature_policy_map = {}
        for feature in self._feature_policy_features:
            # Filter out all the ones not in OT.
            for dependant in [runtime_feature for runtime_feature
                              in feature['depends_on']
                              if not runtime_feature['in_origin_trial']]:
                dependant_name = str(dependant['name'])
                if dependant_name not in runtime_to_feature_policy_map:
                    runtime_to_feature_policy_map[dependant_name] = [feature]
                else:
                    runtime_to_feature_policy_map[dependant_name].append(feature)
        return runtime_to_feature_policy_map

    # Returns a map of feature policy features that depend on OT features to
    # their dependencies. The map is used to generate DisabledByOriginTrial().
    def _make_origin_trial_dependency_map(self):
        origin_trial_dependency_map = {}
        for feature in [fp_feature for fp_feature
                        in self._feature_policy_features
                        if fp_feature['in_origin_trial']]:
            feature_name = str(feature['name'])
            # Only go through the dependencies that are in an origin trial.
            for dependant in [runtime_feature for runtime_feature
                              in feature['depends_on']
                              if runtime_feature['in_origin_trial']]:
                if feature_name in origin_trial_dependency_map:
                    origin_trial_dependency_map[feature_name].append(dependant)
                else:
                    origin_trial_dependency_map[feature_name] = [dependant]
        return origin_trial_dependency_map

    # Sets the 'in_origin_trial' flag for features. A feature is in an origin
    # trial if either itself or one of its dependencies are in an origin trial.
    def _set_in_origin_trial(self):
        # |_dependency_graph| is used to keep the 'depends_on' relationship
        # for the runtime features defined in "runtime_enabled_features.json5".
        self._dependency_graph = util.init_graph(self._runtime_features)
        for feature in self._runtime_features:
            for dependant_name in feature['depends_on']:
                assert dependant_name in self._dependency_graph, dependant_name + ' is not a feature.'
                self._dependency_graph[dependant_name].append(str(feature['name']))
        util.check_if_dependency_graph_contains_cycle(self._dependency_graph)
        util.set_origin_trials_features(self._runtime_features, self._dependency_graph)
        # Set the flag for feature policy features as well.
        for feature in self._feature_policy_features:
            feature['in_origin_trial'] = False
            if any([runtime_feature['in_origin_trial'] for
                    runtime_feature in feature['depends_on']]):
                feature['in_origin_trial'] = True

    def _template_inputs(self):
        return {
            'feature_policy_features': self._feature_policy_features,
            'header_guard': self._header_guard,
            'input_files': self._input_files,
            'runtime_features': self._runtime_features,
            'runtime_to_feature_policy_map': self._runtime_to_feature_policy_map,
            'origin_trial_dependency_map': self._origin_trial_dependency_map,
        }

    @template_expander.use_jinja('templates/' + file_basename + '.cc.tmpl')
    def generate_implementation(self):
        return self._template_inputs()


if __name__ == '__main__':
    json5_generator.Maker(FeaturePolicyFeatureWriter).main()
