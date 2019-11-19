# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import make_runtime_features_utilities as util
from blinkbuild.name_style_converter import NameStyleConverter


class MakeRuntimeFeaturesUtilitiesTest(unittest.TestCase):
    def test_cycle_in_dependency(self):
        # Cycle: 'c' => 'd' => 'e' => 'c'
        graph = {
            'a': ['b'],
            'b': [],
            'c': ['a', 'd'],
            'd': ['e'],
            'e': ['c']
        }
        with self.assertRaises(Exception):
            util.check_if_dependency_graph_contains_cycle(graph)

    def test_in_origin_trials_flag(self):
        features = [
            {'name': NameStyleConverter('a'), 'depends_on': [], 'origin_trial_feature_name': None},
            {'name': NameStyleConverter('b'), 'depends_on': ['a'], 'origin_trial_feature_name': 'OriginTrials'},
            {'name': NameStyleConverter('c'), 'depends_on': ['b'], 'origin_trial_feature_name': None},
            {'name': NameStyleConverter('d'), 'depends_on': ['b'], 'origin_trial_feature_name': None},
            {'name': NameStyleConverter('e'), 'depends_on': ['d'], 'origin_trial_feature_name': None},
        ]
        graph = {
            'a': ['b'],
            'b': ['c', 'd'],
            'c': [],
            'd': ['e'],
            'e': []
        }
        results = [
            {'name': NameStyleConverter('a'), 'in_origin_trial': False},
            {'name': NameStyleConverter('b'), 'depends_on': ['a'],
             'origin_trial_feature_name': 'OriginTrials', 'in_origin_trial': True},
            {'name': NameStyleConverter('c'), 'depends_on': ['b'], 'in_origin_trial': True},
            {'name': NameStyleConverter('d'), 'depends_on': ['b'], 'in_origin_trial': True},
            {'name': NameStyleConverter('e'), 'depends_on': ['d'], 'in_origin_trial': True},
        ]

        util.set_origin_trials_features(features, graph)
        self.assertEqual(len(features), len(results))
        for feature, result in zip(features, results):
            self.assertEqual(result['in_origin_trial'], feature['in_origin_trial'])

    def test_init_graph(self):
        features = [
            {'name': NameStyleConverter('a')},
            {'name': NameStyleConverter('b')},
            {'name': NameStyleConverter('c')},
        ]

        graph = util.init_graph(features)
        self.assertEqual(len(features), len(graph))
        for node in graph:
            self.assertEqual(len(graph[node]), 0)


if __name__ == "__main__":
    unittest.main()
