# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import getopt
import sys

from idl_log import ErrOut, InfoOut, WarnOut

OptionMap = { }


def GetOption(name):
  if name not in OptionMap:
    raise RuntimeError('Could not find option "%s".' % name)
  return OptionMap[name].Get()

class Option(object):
  def __init__(self, name, desc, default = None, callfunc = None,
               testfunc = None, cookie = None):

    # Verify this option is not a duplicate
    if name in OptionMap:
      raise RuntimeError('Option "%s" already exists.' % name)
    self.name = name
    self.desc = desc
    self.default = default
    self.value = default
    self.callfunc = callfunc
    self.testfunc = testfunc
    self.cookie = cookie
    OptionMap[name] = self

  def Set(self, value):
    if self.testfunc:
      if not self.testfunc(self, value): return False
    # If this is a boolean option, set it to true
    if self.default is None:
      self.value = True
    else:
      self.value = value
    if self.callfunc:
      self.callfunc(self)
    return True

  def Get(self):
    return self.value


def DumpOption(option):
  if len(option.name) > 1:
    out = '  --%-15.15s\t%s' % (option.name, option.desc)
  else:
    out = '   -%-15.15s\t%s' % (option.name, option.desc)
  if option.default:
    out = '%s\n\t\t\t(Default: %s)\n' % (out, option.default)
  InfoOut.Log(out)

def DumpHelp(option=None):
  InfoOut.Log('Usage:')
  for opt in sorted(OptionMap.keys()):
    DumpOption(OptionMap[opt])
  sys.exit(0)

#
# Default IDL options
#
# -h : Help, prints options
# --verbose : use verbose output
# --test : test this module
#
Option('h', 'Help', callfunc=DumpHelp)
Option('help', 'Help', callfunc=DumpHelp)
Option('verbose', 'Verbose')
Option('test', 'Test the IDL scripts')

def ParseOptions(args):
  short_opts= ""
  long_opts = []

  # Build short and long option lists
  for name in sorted(OptionMap.keys()):
    option = OptionMap[name]
    if len(name) > 1:
      if option.default is None:
        long_opts.append('%s' % name)
      else:
        long_opts.append('%s=' % name)
    else:
      if option.default is None:
        short_opts += name
      else:
        short_opts += '%s:' % name

  try:
    opts, filenames = getopt.getopt(args, short_opts, long_opts)

    for opt, val in opts:
      if len(opt) == 2: opt = opt[1:]
      if opt[0:2] == '--': opt = opt[2:]
      OptionMap[opt].Set(val)

  except getopt.error as e:
    ErrOut.Log('Illegal option: %s\n' % str(e))
    DumpHelp()
    sys.exit(-1)

  return filenames
