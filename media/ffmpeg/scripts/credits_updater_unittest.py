#!/usr/bin/env python3
#
# Copyright 2015 The Chromium Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import codecs
import copy
import credits_updater as cu
import os
import unittest

# Assumes this script is in ffmpeg/chromium/scripts/
SOURCE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          os.path.pardir, os.path.pardir)
OUTPUT_FILE = 'CREDITS.testing'

# Expected credits for swresample.h applied with the rot13 encoding. Otherwise
# license scanners get confused about the license of this file.
SWRESAMPLE_H_LICENSE_ROT_13 = """yvofjerfnzcyr/fjerfnzcyr.u

Pbclevtug (P) 2011-2013 Zvpunry Avrqreznlre (zvpunryav@tzk.ng)

Guvf svyr vf cneg bs yvofjerfnzcyr

yvofjerfnzcyr vf serr fbsgjner; lbh pna erqvfgevohgr vg naq/be
zbqvsl vg haqre gur grezf bs gur TAH Yrffre Trareny Choyvp
Yvprafr nf choyvfurq ol gur Serr Fbsgjner Sbhaqngvba; rvgure
irefvba 2.1 bs gur Yvprafr, be (ng lbhe bcgvba) nal yngre irefvba.

yvofjerfnzcyr vf qvfgevohgrq va gur ubcr gung vg jvyy or hfrshy,
ohg JVGUBHG NAL JNEENAGL; jvgubhg rira gur vzcyvrq jneenagl bs
ZREPUNAGNOVYVGL be SVGARFF SBE N CNEGVPHYNE CHECBFR.  Frr gur TAH
Yrffre Trareny Choyvp Yvprafr sbe zber qrgnvyf.

Lbh fubhyq unir erprvirq n pbcl bs gur TAH Yrffre Trareny Choyvp
Yvprafr nybat jvgu yvofjerfnzcyr; vs abg, jevgr gb gur Serr Fbsgjner
Sbhaqngvba, Vap., 51 Senaxyva Fgerrg, Svsgu Sybbe, Obfgba, ZN 02110-1301 HFN"""

# The real expected credits for swresample.h.
SWRESAMPLE_H_LICENSE = codecs.decode(SWRESAMPLE_H_LICENSE_ROT_13, 'rot13')


def NewCreditsUpdater():
    return cu.CreditsUpdater(SOURCE_DIR, OUTPUT_FILE)


class CreditsUpdaterUnittest(unittest.TestCase):

    def tearDown(self):
        # Cleanup the testing output file
        test_credits = os.path.join(SOURCE_DIR, OUTPUT_FILE)
        if os.path.exists(test_credits):
            os.remove(test_credits)

    def testNoFiles(self):
        # Write credits without processing any files.
        NewCreditsUpdater().WriteCredits()

        # Credits should *always* have LICENSE.md followed by full LGPL text.
        expected_lines = NormalizeNewLines(GetLicenseMdLines() +
                                           GetSeparatorLines() +
                                           GetLicenseLines(cu.License.LGPL))
        credits_lines = ReadCreditsLines()
        self.assertEqual(expected_lines, credits_lines)

    def testLPGLFiles(self):
        # Process two known LGPL files
        updater = NewCreditsUpdater()
        updater.ProcessFile('libavformat/mp3dec.c')
        updater.ProcessFile('libavformat/mp3enc.c')
        updater.WriteCredits()

        # Expect output to have just LGPL text (once) preceded by LICENSE.md
        expected_lines = NormalizeNewLines(GetLicenseMdLines() +
                                           GetSeparatorLines() +
                                           GetLicenseLines(cu.License.LGPL))
        credits_lines = ReadCreditsLines()
        self.assertEqual(expected_lines, credits_lines)

    def testKnownBucketFiles(self):
        # Process some JPEG and MIPS files.
        updater = NewCreditsUpdater()
        updater.ProcessFile('libavcodec/jfdctfst.c')
        updater.ProcessFile('libavutil/mips/float_dsp_mips.c')
        updater.WriteCredits()

        # Expected output to have JPEG and MIPS text in addition to the typical LGPL
        # and LICENSE.md header. JPEG should appear before MIPS because known
        # buckets will be printed in alphabetical order.
        expected_lines = NormalizeNewLines(
            GetLicenseMdLines() + GetSeparatorLines() +
            ['libavcodec/jfdctfst.c\n\n'] + GetLicenseLines(cu.License.JPEG) +
            GetSeparatorLines() + ['libavutil/mips/float_dsp_mips.c\n\n'] +
            GetLicenseLines(cu.License.MIPS) + GetSeparatorLines() +
            GetLicenseLines(cu.License.LGPL))
        credits_lines = ReadCreditsLines()
        self.assertEqual(expected_lines, credits_lines)

    def testGeneratedAndKnownLicences(self):
        # Process a file that doesn't fall into a known bucket (e.g. the license
        # header for this file is unique). Also process a known bucket file.
        updater = NewCreditsUpdater()
        updater.ProcessFile('libswresample/swresample.h')
        updater.ProcessFile('libavutil/mips/float_dsp_mips.c')
        updater.WriteCredits()

        # Expect output to put swresample.h header first, followed by MIPS.
        expected_lines = NormalizeNewLines(
            GetLicenseMdLines() + GetSeparatorLines() +
            SWRESAMPLE_H_LICENSE.splitlines(True) + GetSeparatorLines() +
            ['libavutil/mips/float_dsp_mips.c\n\n'] +
            GetLicenseLines(cu.License.MIPS) + GetSeparatorLines() +
            GetLicenseLines(cu.License.LGPL))
        credits_lines = ReadCreditsLines()
        self.assertEqual(expected_lines, credits_lines)

    def testGeneratedLicencesOrder(self):
        # Process files that do not fall into a known bucket and assert that their
        # licenses are listed in alphabetical order of the file names.
        files = [
            'libswresample/swresample.h',
            'libavcodec/arm/jrevdct_arm.S',
            'libavcodec/mips/celp_math_mips.c',
            'libavcodec/mips/acelp_vectors_mips.c',
            'libavformat/oggparsetheora.c',
            'libavcodec/x86/xvididct.asm',
        ]
        updater = NewCreditsUpdater()
        for f in files:
            updater.ProcessFile(f)
        updater.WriteCredits()

        credits = ''.join(ReadCreditsLines())
        current_offset = 0
        for f in sorted(files):
            i = credits.find(f, current_offset)
            if i == -1:
                self.fail(
                    "Failed to find %s starting at offset %s of content:\n%s" %
                    (f, current_offset, credits))
            current_offset = i + len(f)

    def testKnownFileDigestChange(self):
        updater = NewCreditsUpdater()

        # Choose a known file.
        known_file = os.path.join('libavformat', 'oggparseogm.c')
        self.assertTrue(known_file in updater.known_file_map)

        # Show file processing works without raising SystemExit.
        updater.ProcessFile(known_file)

        # Alter the license digest for this file to simulate a change to the
        # license header.
        orig_file_info = updater.known_file_map[known_file]
        altered_file_info = cu.FileInfo(
            cu.License.LGPL, 'chris' + orig_file_info.license_digest[5:])
        updater.known_file_map[known_file] = altered_file_info

        # Verify digest mismatch triggers SystemExit.
        with self.assertRaises(SystemExit):
            updater.ProcessFile(known_file)


# Globals to cache the text of static files once read.
g_license_md_lines = []
g_license_lines = {}


def ReadCreditsLines():
    with open(os.path.join(SOURCE_DIR, OUTPUT_FILE)) as test_credits:
        return test_credits.readlines()


def GetLicenseMdLines():
    global g_license_md_lines
    if not len(g_license_md_lines):
        with open(os.path.join(SOURCE_DIR,
                               cu.UPSTREAM_LICENSEMD)) as license_md:
            g_license_md_lines = license_md.readlines()
    return g_license_md_lines


def GetLicenseLines(license_file):
    if not license_file in g_license_lines:
        g_license_lines[license_file] = GetFileLines(
            os.path.join(cu.LICENSE_TEXTS[license_file]))
    return g_license_lines[license_file]


def GetFileLines(file_path):
    with open(file_path) as open_file:
        return open_file.readlines()


def GetSeparatorLines():
    # Pass True to preserve \n chars in the return.
    return cu.LICENSE_SEPARATOR.splitlines(True)


# Combine into a string then split back out to a list. This is important for
# making constructed expectations match the credits read from a file. E.g.
# input: ['foo', '\n', 'bar']
# return: ['foo\n', 'bar']
# Comparing lists line by line makes for much better diffs when things go wrong.


def NormalizeNewLines(lines):
    return ''.join(lines).splitlines(True)


if __name__ == '__main__':
    unittest.main()
