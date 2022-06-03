# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

import make_runtime_features_utilities as util
from blinkbuild.name_style_converter import NameStyleConverter


def _feature(name,
             depends_on=[],
             implied_by=[],
             origin_trial_feature_name=None):
    return {
        'name': name,
        'depends_on': depends_on,
        'implied_by': implied_by,
        'origin_trial_feature_name': origin_trial_feature_name
    }


class MakeRuntimeFeaturesUtilitiesTest(unittest.TestCase):
    def test_cycle(self):
        # Cycle: 'c' => 'd' => 'e' => 'c'
        with self.assertRaisesRegexp(
                AssertionError, 'Cycle found in depends_on/implied_by graph'):
            util.origin_trials([
                _feature('a', depends_on=['b']),
                _feature('b'),
                _feature('c', implied_by=['a', 'd']),
                _feature('d', depends_on=['e']),
                _feature('e', implied_by=['c'])
            ])

    def test_bad_dependency(self):
        with self.assertRaisesRegexp(AssertionError,
                                     'a: Depends on non-existent-feature: x'):
            util.origin_trials([_feature('a', depends_on=['x'])])

    def test_bad_implication(self):
        with self.assertRaisesRegexp(AssertionError,
                                     'a: Implied by non-existent-feature: x'):
            util.origin_trials([_feature('a', implied_by=['x'])])
        with self.assertRaisesRegexp(
                AssertionError,
                'a: A feature must be in origin trial if implied by an origin trial feature: b'
        ):
            util.origin_trials([
                _feature('a', implied_by=['b']),
                _feature('b', origin_trial_feature_name='b')
            ])

    def test_both_dependency_and_implication(self):
        with self.assertRaisesRegexp(
                AssertionError,
                'c: Only one of implied_by and depends_on is allowed'):
            util.origin_trials([
                _feature('a'),
                _feature('b'),
                _feature('c', depends_on=['a'], implied_by=['b'])
            ])

    def test_origin_trials(self):
        features = [
            _feature(NameStyleConverter('a')),
            _feature(
                NameStyleConverter('b'),
                depends_on=['a'],
                origin_trial_feature_name='b'),
            _feature(NameStyleConverter('c'), depends_on=['b']),
            _feature(NameStyleConverter('d'), depends_on=['b']),
            _feature(NameStyleConverter('e'), depends_on=['d'])
        ]
        self.assertSetEqual(util.origin_trials(features), {'b', 'c', 'd', 'e'})

        features = [
            _feature('a'),
            _feature('b', depends_on=['x', 'y']),
            _feature('c', depends_on=['y', 'z']),
            _feature('x', depends_on=['a']),
            _feature('y', depends_on=['x'], origin_trial_feature_name='y'),
            _feature('z', depends_on=['y'])
        ]
        self.assertSetEqual(util.origin_trials(features), {'b', 'c', 'y', 'z'})


if __name__ == "__main__":
    unittest.main()
