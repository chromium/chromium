#!/usr/bin/env python
# Copyright (C) 2015 Google Inc. All rights reserved.
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

import json5_generator
import make_runtime_features
import name_utilities
import template_expander


# We want exactly the same parsing as RuntimeFeatureWriter
# but generate different files.
class OriginTrialsWriter(make_runtime_features.BaseRuntimeFeatureWriter):
    file_basename = 'origin_trials'

    def __init__(self, json5_file_path, output_dir):
        super(OriginTrialsWriter, self).__init__(json5_file_path, output_dir)
        self._outputs = {
            (self.file_basename + '.cc'): self.generate_implementation,
        }
        self._implied_mappings = self._make_implied_mappings()
        self._trial_to_features_map = self._make_trial_to_features_map()
        self._max_features_per_trial = max(
            len(features) for features in self._trial_to_features_map.values())
        self._set_trial_types()

    @property
    def origin_trial_features(self):
        return self._origin_trial_features

    def _make_implied_mappings(self):
        # Set up the implied_by relationships between trials.
        implied_mappings = dict()
        for implied_feature in (feature
                                for feature in self._origin_trial_features
                                if feature['origin_trial_feature_name']
                                and feature['implied_by']):
            # An origin trial can only be implied by other features that also
            # have a trial defined.
            implied_by_trials = []
            for implied_by_name in implied_feature['implied_by']:
                if any(implied_by_name == feature['name'].original
                       and feature['origin_trial_feature_name']
                       for feature in self._origin_trial_features):

                    implied_by_trials.append(implied_by_name)

                    # Keep a set of origin trial features implied for each
                    # trial. This is essentially an inverse of the implied_by
                    # list attached to each feature.
                    implied_set = implied_mappings.get(implied_by_name)
                    if implied_set is None:
                        implied_set = set()
                        implied_mappings[implied_by_name] = implied_set
                    implied_set.add(implied_feature['name'].original)

            implied_feature['implied_by_origin_trials'] = implied_by_trials

        return implied_mappings

    def _make_trial_to_features_map(self):
        trial_feature_mappings = {}
        for feature in [
                feature for feature in self._origin_trial_features
                if feature['origin_trial_feature_name']
        ]:
            trial_name = feature['origin_trial_feature_name']
            if trial_name in trial_feature_mappings:
                trial_feature_mappings[trial_name].append(feature)
            else:
                trial_feature_mappings[trial_name] = [feature]
        return trial_feature_mappings

    def _set_trial_types(self):
        for feature in self._origin_trial_features:
            trial_type = feature['origin_trial_type']
            if feature[
                    'origin_trial_allows_insecure'] and trial_type != 'deprecation':
                raise Exception('Origin trial must have type deprecation to '
                                'specify origin_trial_allows_insecure: %s' %
                                feature['name'])
            if trial_type:
                feature[
                    'origin_trial_type'] = name_utilities._upper_camel_case(
                        trial_type)

    @template_expander.use_jinja('templates/' + file_basename + '.cc.tmpl')
    def generate_implementation(self):
        return {
            'features': self._features,
            'origin_trial_features': self._origin_trial_features,
            'implied_origin_trial_features': self._implied_mappings,
            'trial_to_features_map': self._trial_to_features_map,
            'max_features_per_trial': self._max_features_per_trial,
            'input_files': self._input_files,
        }


if __name__ == '__main__':
    json5_generator.Maker(OriginTrialsWriter).main()
