#!/usr/bin/env python3
#
# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittest for generate_gn.py.

It's tough to test the lower-level GetSourceFiles() and GetObjectFiles()
functions, so this focuses on the higher-level functions assuming those two
functions are working as intended (i.e., producing lists of files).
"""

import generate_gn as gg
from generate_gn import SourceSet, SourceListCondition
import unittest
from os import path


class ModuleUnittest(unittest.TestCase):

    def testGetObjectToSourceMapping(self):
        srcs = [
            'a.c',
            'b.asm',
            'c.cc',
        ]
        expected = {
            'a.o': 'a.c',
            'b.o': 'b.asm',
            'c.o': 'c.cc',
        }
        self.assertEqual(expected, gg.GetObjectToSourceMapping(srcs))

    def testGetSourceFileSet(self):
        objs_to_srcs = {
            'a.o': 'a.c',
            'b.o': 'b.asm',
            'c.o': 'c.cc',
        }
        objs = [
            'a.o',
            'c.o',
        ]
        expected = set(['a.c', 'c.cc'])
        self.assertEqual(expected, gg.GetSourceFileSet(objs_to_srcs, objs))

    def testGetSourceFileSet_NotFound(self):
        objs_to_srcs = {
            'a.o': 'a.c',
            'b.o': 'b.asm',
            'c.o': 'c.cc',
        }
        objs = [
            'd.o',
        ]
        self.assertRaises(KeyError, gg.GetSourceFileSet, objs_to_srcs, objs)


class SourceSetUnittest(unittest.TestCase):

    def testEquals(self):
        a = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('1', '2', '3')]))
        b = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('1', '2', '3')]))
        c = SourceSet(set(['c', 'd']),
                      set([SourceListCondition('1', '2', '3')]))
        d = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('0', '2', '3')]))
        e = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('1', '0', '3')]))
        f = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('1', '2', '0')]))

        self.assertEqual(a, b)
        self.assertNotEqual(a, c)
        self.assertNotEqual(a, d)
        self.assertNotEqual(a, e)
        self.assertNotEqual(a, f)

    def testIntersect_Exact(self):
        a = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('1', '2', '3')]))
        b = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('3', '4', '6')]))

        c = a.Intersect(b)

        self.assertEqual(c.sources, set(['a', 'b']))
        self.assertEqual(
            c.conditions,
            set([
                SourceListCondition('1', '2', '3'),
                SourceListCondition('3', '4', '6')
            ]))
        self.assertFalse(c.IsEmpty())

    def testIntersect_Disjoint(self):
        a = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('1', '2', '3')]))
        b = SourceSet(set(['c', 'd']),
                      set([SourceListCondition('3', '4', '6')]))

        c = a.Intersect(b)

        self.assertEqual(c.sources, set())
        self.assertEqual(
            c.conditions,
            set([
                SourceListCondition('1', '2', '3'),
                SourceListCondition('3', '4', '6')
            ]))
        self.assertTrue(c.IsEmpty())

    def testIntersect_Overlap(self):
        a = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('1', '2', '3')]))
        b = SourceSet(set(['b', 'c']),
                      set([SourceListCondition('3', '4', '6')]))

        c = a.Intersect(b)

        self.assertEqual(c.sources, set(['b']))
        self.assertEqual(
            c.conditions,
            set([
                SourceListCondition('1', '2', '3'),
                SourceListCondition('3', '4', '6')
            ]))
        self.assertFalse(c.IsEmpty())

    def testDifference_Exact(self):
        a = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('1', '2', '3')]))
        b = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('1', '2', '3')]))

        c = a.Difference(b)

        self.assertEqual(c.sources, set())
        self.assertEqual(c.conditions,
                         set([SourceListCondition('1', '2', '3')]))
        self.assertTrue(c.IsEmpty())

    def testDifference_Disjoint(self):
        a = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('1', '2', '3')]))
        b = SourceSet(set(['c', 'd']),
                      set([SourceListCondition('3', '4', '6')]))

        c = a.Difference(b)

        self.assertEqual(c.sources, set(['a', 'b']))
        self.assertEqual(c.conditions, set())
        self.assertTrue(c.IsEmpty())

    def testDifference_Overlap(self):
        a = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('1', '2', '5')]))
        b = SourceSet(
            set(['b', 'c', 'd']),
            set([
                SourceListCondition('1', '2', '5'),
                SourceListCondition('3', '4', '6')
            ]))

        c = a.Difference(b)

        self.assertEqual(c.sources, set(['a']))
        self.assertEqual(c.conditions,
                         set([SourceListCondition('1', '2', '5')]))
        self.assertFalse(c.IsEmpty())

    def testGenerateGnStanza(self):
        # ia32 should be x86.  Win should appear as an OS restriction.
        a = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('ia32', 'Chromium', 'win')]))
        a_stanza = a.GenerateGnStanza()
        a_stanza.index('current_cpu == "x86"')
        a_stanza.index('is_win')

        # x64 should just be x64.  Linux should appear as an OS restriction.
        b = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('x64', 'Chromium', 'linux')]))
        b_stanza = b.GenerateGnStanza()
        b_stanza.index('current_cpu == "x64"')
        b_stanza.index('use_linux_config')

        # arm should just be arm.
        c = SourceSet(set(['a', 'b']),
                      set([SourceListCondition('arm', 'Chromium', 'linux')]))
        c_stanza = c.GenerateGnStanza()
        c_stanza.index('current_cpu == "arm"')

        # arm-neon should be arm and flip the arm_neon switch.
        d = SourceSet(
            set(['a', 'b']),
            set([SourceListCondition('arm-neon', 'Chromium', 'linux')]))
        d_stanza = d.GenerateGnStanza()
        d_stanza.index('current_cpu == "arm" && arm_use_neon')

        # Multiple conditions
        e = SourceSet(
            set(['a', 'b']),
            set([
                SourceListCondition('arm', 'Chrome', 'win'),
                SourceListCondition('x64', 'Chromium', 'linux')
            ]))
        e_stanza = e.GenerateGnStanza()
        e_stanza.index(('is_win && current_cpu == "arm"'
                        ' && ffmpeg_branding == "Chrome"'))
        e_stanza.index(('use_linux_config && current_cpu == "x64"'
                        ' && ffmpeg_branding == "Chromium"'))

        # mac should imply is_apple.
        f = SourceSet(set(['a']),
                      set([SourceListCondition('arm64', 'Chromium', 'mac')]))
        f_stanza = f.GenerateGnStanza()
        f_stanza.index('is_apple')

    def testComplexSourceListConditions(self):
        # Create 2 sets with intersecting source 'a', but setup such that 'a'
        # is only valid for combinations (x86 && windows) || (x64 && linux). The
        # generated gn stanza should then not allow for inclusion of the 'a' file
        # for combinations like x86 && linux.
        a = SourceSet(set(['a']), set([SourceListCondition('x86', 'c',
                                                           'win')]))
        b = SourceSet(set(['a']),
                      set([SourceListCondition('x64', 'c', 'linux')]))
        disjoint_sets = gg.CreatePairwiseDisjointSets([a, b])

        # This condition is bad because x86 && linux would pass. Admittedly a very
        # fragile way to test this, but evaluating gn stanzas is hard, and it at
        # least serves to document the motivation for the associated changes to
        # our generate_gn.py
        bad_condition = ('(current_cpu == "x86" || current_cpu == "x64")'
                         ' && (ffmpeg_branding == "c")'
                         ' && (is_win || is_linux || is_chromeos)')

        # Expect only a single set since the two original sets have the same source
        # list.
        self.assertEqual(1, len(disjoint_sets))

        stanza = disjoint_sets[0].GenerateGnStanza()
        self.assertEqual(stanza.find(bad_condition), -1)

    def assertEqualSourceSets(self, expected, actual):
        assert all(isinstance(a, SourceSet) for a in expected)
        assert all(isinstance(a, SourceSet) for a in actual)

        def SourceSetToString(source_set):
            sources = [str(e) for e in source_set.sources]
            conditions = [str(e) for e in source_set.conditions]
            sources_str = ','.join(sources)
            conditions_str = '\n\t'.join(conditions)
            return '  sources:%s\n  cs:\t%s' % (sources_str, conditions_str)

        missing_elements = expected.difference(actual)
        extra_elements = actual.difference(expected)
        msg = ''
        if len(missing_elements):
            msg += 'Missing expected elements:\n'
            for e in missing_elements:
                msg += SourceSetToString(e) + '\n'
        if len(extra_elements):
            msg += 'Found extra elements:\n'
            for e in extra_elements:
                msg += SourceSetToString(e) + '\n'

        self.assertTrue(expected == actual, msg=msg)

    def testCreatePairwiseDisjointSets_Pair(self):
        a = SourceSet(set(['common', 'intel']),
                      set([SourceListCondition('ia32', 'Chromium', 'win')]))
        b = SourceSet(set(['common', 'intel', 'chrome']),
                      set([SourceListCondition('ia32', 'Chrome', 'win')]))

        expected = set()
        expected.add(
            SourceSet(
                set(['common', 'intel']),
                set([
                    SourceListCondition('ia32', 'Chromium', 'win'),
                    SourceListCondition('ia32', 'Chrome', 'win')
                ])))
        expected.add(
            SourceSet(set(['chrome']),
                      set([SourceListCondition('ia32', 'Chrome', 'win')])))

        source_sets = gg.CreatePairwiseDisjointSets([a, b])
        self.assertEqualSourceSets(expected, set(source_sets))

    def testCreatePairwiseDisjointSets_Triplet(self):
        a = SourceSet(set(['common', 'intel']),
                      set([SourceListCondition('ia32', 'Chromium', 'win')]))
        b = SourceSet(set(['common', 'intel', 'chrome']),
                      set([SourceListCondition('x64', 'Chrome', 'win')]))
        c = SourceSet(set(['common', 'arm']),
                      set([SourceListCondition('arm', 'Chromium', 'win')]))

        expected = set()
        expected.add(
            SourceSet(
                set(['common']),
                set([
                    SourceListCondition('ia32', 'Chromium', 'win'),
                    SourceListCondition('x64', 'Chrome', 'win'),
                    SourceListCondition('arm', 'Chromium', 'win')
                ])))
        expected.add(
            SourceSet(
                set(['intel']),
                set([
                    SourceListCondition('ia32', 'Chromium', 'win'),
                    SourceListCondition('x64', 'Chrome', 'win')
                ])))
        expected.add(
            SourceSet(set(['chrome']),
                      set([SourceListCondition('x64', 'Chrome', 'win')])))
        expected.add(
            SourceSet(set(['arm']),
                      set([SourceListCondition('arm', 'Chromium', 'win')])))

        source_sets = gg.CreatePairwiseDisjointSets([a, b, c])
        self.assertEqualSourceSets(expected, set(source_sets))

    def testCreatePairwiseDisjointSets_Multiple(self):
        a = SourceSet(set(['common', 'intel']),
                      set([SourceListCondition('ia32', 'Chromium', 'linux')]))
        b = SourceSet(set(['common', 'intel', 'chrome']),
                      set([SourceListCondition('ia32', 'Chrome', 'linux')]))
        c = SourceSet(set(['common', 'intel']),
                      set([SourceListCondition('x64', 'Chromium', 'linux')]))
        d = SourceSet(set(['common', 'intel', 'chrome']),
                      set([SourceListCondition('x64', 'Chrome', 'linux')]))
        e = SourceSet(set(['common', 'arm']),
                      set([SourceListCondition('arm', 'Chromium', 'linux')]))
        f = SourceSet(
            set(['common', 'arm-neon', 'chrome']),
            set([SourceListCondition('arm-neon', 'Chrome', 'linux')]))

        expected = set()
        expected.add(
            SourceSet(
                set(['common']),
                set([
                    SourceListCondition('ia32', 'Chromium', 'linux'),
                    SourceListCondition('ia32', 'Chrome', 'linux'),
                    SourceListCondition('x64', 'Chromium', 'linux'),
                    SourceListCondition('x64', 'Chrome', 'linux'),
                    SourceListCondition('arm', 'Chromium', 'linux'),
                    SourceListCondition('arm-neon', 'Chrome', 'linux')
                ])))
        expected.add(
            SourceSet(
                set(['intel']),
                set([
                    SourceListCondition('ia32', 'Chromium', 'linux'),
                    SourceListCondition('ia32', 'Chrome', 'linux'),
                    SourceListCondition('x64', 'Chromium', 'linux'),
                    SourceListCondition('x64', 'Chrome', 'linux')
                ])))
        expected.add(
            SourceSet(set(['arm']),
                      set([SourceListCondition('arm', 'Chromium', 'linux')])))
        expected.add(
            SourceSet(
                set(['chrome']),
                set([
                    SourceListCondition('ia32', 'Chrome', 'linux'),
                    SourceListCondition('x64', 'Chrome', 'linux'),
                    SourceListCondition('arm-neon', 'Chrome', 'linux')
                ])))
        expected.add(
            SourceSet(
                set(['arm-neon']),
                set([SourceListCondition('arm-neon', 'Chrome', 'linux')])))
        source_sets = gg.CreatePairwiseDisjointSets([a, b, c, d, e, f])
        self.assertEqualSourceSets(expected, set(source_sets))

    def testReduceConditions(self):
        # Set conditions span all of the supported architectures for linux.
        a = SourceSet(
            set(['foo.c']),
            set([
                SourceListCondition('ia32', 'Chromium', 'linux'),
                SourceListCondition('x64', 'Chromium', 'linux'),
                SourceListCondition('arm', 'Chromium', 'linux'),
                SourceListCondition('arm64', 'Chromium', 'linux'),
                SourceListCondition('arm-neon', 'Chromium', 'linux'),
            ]))
        gg.ReduceConditionalLogic(a)

        # Conditions should reduce to a single condition with wild-card for arch.
        expected = set([SourceListCondition('*', 'Chromium', 'linux')])
        self.assertEqual(expected, a.conditions)

        # Set conditions span all of the supported architectures for windows.
        b = SourceSet(
            set(['foo.c']),
            set([
                SourceListCondition('ia32', 'Chromium', 'win'),
                SourceListCondition('x64', 'Chromium', 'win'),
                SourceListCondition('arm64', 'Chromium', 'win'),
            ]))
        gg.ReduceConditionalLogic(b)

        # Conditions should reduce to a single condition with wild-card for
        expected = set([SourceListCondition('*', 'Chromium', 'win')])
        self.assertEqual(expected, b.conditions)

        # Set conditions span all supported architectures and brandings for windows.
        b = SourceSet(
            set(['foo.c']),
            set([
                SourceListCondition('ia32', 'Chromium', 'win'),
                SourceListCondition('x64', 'Chromium', 'win'),
                SourceListCondition('arm64', 'Chromium', 'win'),
                SourceListCondition('ia32', 'Chrome', 'win'),
                SourceListCondition('x64', 'Chrome', 'win'),
                SourceListCondition('arm64', 'Chrome', 'win'),
            ]))
        gg.ReduceConditionalLogic(b)
        expected = set([SourceListCondition('*', '*', 'win')])
        self.assertEqual(expected, b.conditions)

        # Set conditions span all supported platforms.
        c = SourceSet(
            set(['foo.c']),
            set([
                SourceListCondition('x64', 'Chromium', 'win'),
                SourceListCondition('x64', 'Chromium', 'mac'),
                SourceListCondition('x64', 'Chromium', 'linux'),
                SourceListCondition('x64', 'Chromium', 'android'),
            ]))
        gg.ReduceConditionalLogic(c)
        expected = set([SourceListCondition('x64', 'Chromium', '*')])
        self.assertEqual(expected, c.conditions)

        # Spans all architectures for Chromium, but also all targets for ia32 & win.
        d = SourceSet(
            set(['foo.c']),
            set([
                SourceListCondition('arm64', 'Chromium', 'win'),
                SourceListCondition('x64', 'Chromium', 'win'),
                SourceListCondition('ia32', 'Chromium', 'win'),
                SourceListCondition('ia32', 'Chrome', 'win'),
            ]))
        gg.ReduceConditionalLogic(d)
        expected = set([
            SourceListCondition('*', 'Chromium', 'win'),
            SourceListCondition('ia32', '*', 'win'),
        ])
        self.assertEqual(expected, d.conditions)

    def testReduceConditions_fullSpan(self):
        # Build SourceSet with conditions spanning every combination of attributes.
        ss = SourceSet(set(['foo.c']), set())
        for arch in gg.SUPPORT_MATRIX[gg.Attr.ARCHITECTURE]:
            for target in gg.SUPPORT_MATRIX[gg.Attr.TARGET]:
                for platform in gg.SUPPORT_MATRIX[gg.Attr.PLATFORM]:
                    ss.conditions.add(
                        SourceListCondition(arch, target, platform))

        gg.ReduceConditionalLogic(ss)
        expected = set([SourceListCondition('*', '*', '*')])
        self.assertEqual(expected, ss.conditions)

    def testGenerateStanzaWildCard(self):
        a = SourceSet(set(['foo.c']),
                      set([SourceListCondition('x64', 'Chromium', '*')]))
        stanza = a.GenerateGnStanza()
        stanza.index('== "x64"')
        stanza.index('ffmpeg_branding == "Chromium"')
        # OS is wild-card, so it should not be mentioned in the stanza.
        self.assertEqual(-1, stanza.find('OS =='))

    def testFixObjectBasenameCollisions(self):
        # Use callback to capture executed renames.
        observed_renames = set()

        def do_rename_cb(old_path, new_path, content):
            observed_renames.add((old_path, new_path))

        # Verify basic rename case - same basename in different directories.
        a = SourceSet(set(['foo.c']), set([SourceListCondition('*', '*',
                                                               '*')]))
        b = SourceSet(set([path.join('a', 'foo.c'),
                           path.join('b', 'foo.c')]),
                      set([SourceListCondition('*', '*', '*')]))
        expected_renames = set([
            (path.join('a', 'foo.c'), path.join('a', 'autorename_a_foo.c')),
            (path.join('b', 'foo.c'), path.join('b', 'autorename_b_foo.c'))
        ])
        gg.FixObjectBasenameCollisions([a, b], [],
                                       do_rename_cb,
                                       log_renames=False)
        self.assertEqual(expected_renames, observed_renames)

        # Verify renames file extensions in same and different directory.
        observed_renames = set()
        a = SourceSet(set(['foo.c']), set([SourceListCondition('*', '*',
                                                               '*')]))
        b = SourceSet(set(['foo.asm']),
                      set([SourceListCondition('*', '*', '*')]))
        c = SourceSet(
            set([path.join('a', 'foo.S'),
                 path.join('b', 'foo.asm')]),
            set([SourceListCondition('*', '*', '*')]))
        expected_renames = set([
            ('foo.asm', 'autorename_foo.asm'),
            (path.join('a', 'foo.S'), path.join('a', 'autorename_a_foo.S')),
            (path.join('b', 'foo.asm'), path.join('b', 'autorename_b_foo.asm'))
        ])
        gg.FixObjectBasenameCollisions([a, b, c], [],
                                       do_rename_cb,
                                       log_renames=False)
        self.assertEqual(expected_renames, observed_renames)


if __name__ == '__main__':
    unittest.main()
