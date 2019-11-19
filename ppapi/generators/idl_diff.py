#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import glob
import os
import subprocess
import sys

from idl_option import GetOption, Option, ParseOptions
from idl_outfile import IDLOutFile
#
# IDLDiff
#
# IDLDiff is a tool for comparing sets of IDL generated header files
# with the standard checked in headers.  It does this by capturing the
# output of the standard diff tool, parsing it into separate changes, then
# ignoring changes that are know to be safe, such as adding or removing
# blank lines, etc...
#

Option('gen', 'IDL generated files', default='hdir')
Option('src', 'Original ".h" files', default='../c')
Option('halt', 'Stop if a difference is found')
Option('diff', 'Directory holding acceptable diffs', default='diff')
Option('ok', 'Write out the diff file.')
# Change
#
# A Change object contains the previous lines, new news and change type.
#
class Change(object):
  def __init__(self, mode, was, now):
    self.mode = mode
    self.was = was
    self.now = now

  def Dump(self):
    if not self.was:
      print('Adding %s' % self.mode)
    elif not self.now:
      print('Missing %s' % self.mode)
    else:
      print('Modifying %s' % self.mode)

    for line in self.was:
      print('src: >>%s<<' % line)
    for line in self.now:
      print('gen: >>%s<<' % line)
    print

#
# IsCopyright
#
# Return True if this change is only a one line change in the copyright notice
# such as non-matching years.
#
def IsCopyright(change):
  if len(change.now) != 1 or len(change.was) != 1: return False
  if 'Copyright (c)' not in change.now[0]: return False
  if 'Copyright (c)' not in change.was[0]: return False
  return True

#
# IsBlankComment
#
# Return True if this change only removes a blank line from a comment
#
def IsBlankComment(change):
  if change.now: return False
  if len(change.was) != 1: return False
  if change.was[0].strip() != '*': return False
  return True

#
# IsBlank
#
# Return True if this change only adds or removes blank lines
#
def IsBlank(change):
  for line in change.now:
    if line: return False
  for line in change.was:
    if line: return False
  return True


#
# IsCppComment
#
# Return True if this change only going from C++ to C style
#
def IsToCppComment(change):
  if not len(change.now) or len(change.now) != len(change.was):
    return False
  for index in range(len(change.now)):
    was = change.was[index].strip()
    if was[:2] != '//':
      return False
    was = was[2:].strip()
    now = change.now[index].strip()
    if now[:2] != '/*':
      return False
    now = now[2:-2].strip()
    if now != was:
      return False
  return True


  return True

def IsMergeComment(change):
  if len(change.was) != 1: return False
  if change.was[0].strip() != '*': return False
  for line in change.now:
    stripped = line.strip()
    if stripped != '*' and stripped[:2] != '/*' and stripped[-2:] != '*/':
      return False
  return True
#
# IsSpacing
#
# Return True if this change is only different in the way 'words' are spaced
# such as in an enum:
#   ENUM_XXX   = 1,
#   ENUM_XYY_Y = 2,
# vs
#   ENUM_XXX = 1,
#   ENUM_XYY_Y = 2,
#
def IsSpacing(change):
  if len(change.now) != len(change.was): return False
  for i in range(len(change.now)):
    # Also ignore right side comments
    line = change.was[i]
    offs = line.find('//')
    if offs == -1:
      offs = line.find('/*')
    if offs >-1:
      line = line[:offs-1]

    words1 = change.now[i].split()
    words2 = line.split()
    if words1 != words2: return False
  return True

#
# IsInclude
#
# Return True if change has extra includes
#
def IsInclude(change):
  for line in change.was:
    if line.strip().find('struct'): return False
  for line in change.now:
    if line and '#include' not in line: return False
  return True

#
# IsCppComment
#
# Return True if the change is only missing C++ comments
#
def IsCppComment(change):
  if len(change.now): return False
  for line in change.was:
    line = line.strip()
    if line[:2] != '//': return False
  return True
#
# ValidChange
#
# Return True if none of the changes does not patch an above "bogus" change.
#
def ValidChange(change):
  if IsToCppComment(change): return False
  if IsCopyright(change): return False
  if IsBlankComment(change): return False
  if IsMergeComment(change): return False
  if IsBlank(change): return False
  if IsSpacing(change): return False
  if IsInclude(change): return False
  if IsCppComment(change): return False
  return True


#
# Swapped
#
# Check if the combination of last + next change signals they are both
# invalid such as swap of line around an invalid block.
#
def Swapped(last, next):
  if not last.now and not next.was and len(last.was) == len(next.now):
    cnt = len(last.was)
    for i in range(cnt):
      match = True
      for j in range(cnt):
        if last.was[j] != next.now[(i + j) % cnt]:
          match = False
          break;
      if match: return True
  if not last.was and not next.now and len(last.now) == len(next.was):
    cnt = len(last.now)
    for i in range(cnt):
      match = True
      for j in range(cnt):
        if last.now[i] != next.was[(i + j) % cnt]:
          match = False
          break;
      if match: return True
  return False


def FilterLinesIn(output):
  was = []
  now = []
  filter = []
  for index in range(len(output)):
    filter.append(False)
    line = output[index]
    if len(line) < 2: continue
    if line[0] == '<':
      if line[2:].strip() == '': continue
      was.append((index, line[2:]))
    elif line[0] == '>':
      if line[2:].strip() == '': continue
      now.append((index, line[2:]))
  for windex, wline in was:
    for nindex, nline in now:
      if filter[nindex]: continue
      if filter[windex]: continue
      if wline == nline:
        filter[nindex] = True
        filter[windex] = True
        if GetOption('verbose'):
          print("Found %d, %d >>%s<<" % (windex + 1, nindex + 1, wline))
  out = []
  for index in range(len(output)):
    if not filter[index]:
      out.append(output[index])

  return out
#
# GetChanges
#
# Parse the output into discrete change blocks.
#
def GetChanges(output):
  # Split on lines, adding an END marker to simply add logic
  lines = output.split('\n')
  lines = FilterLinesIn(lines)
  lines.append('END')

  changes = []
  was = []
  now = []
  mode = ''
  last = None

  for line in lines:
    #print("LINE=%s" % line)
    if not line: continue

    elif line[0] == '<':
      if line[2:].strip() == '': continue
      # Ignore prototypes
      if len(line) > 10:
        words = line[2:].split()
        if len(words) == 2 and words[1][-1] == ';':
          if words[0] == 'struct' or words[0] == 'union':
            continue
      was.append(line[2:])
    elif line[0] == '>':
      if line[2:].strip() == '': continue
      if line[2:10] == '#include': continue
      now.append(line[2:])
    elif line[0] == '-':
      continue
    else:
      change = Change(line, was, now)
      was = []
      now = []
      if ValidChange(change):
        changes.append(change)
      if line == 'END':
        break

  return FilterChanges(changes)

def FilterChanges(changes):
  if len(changes) < 2: return changes
  out = []
  filter = [False for change in changes]
  for cur in range(len(changes)):
    for cmp in range(cur+1, len(changes)):
      if filter[cmp]:
        continue
      if Swapped(changes[cur], changes[cmp]):
        filter[cur] = True
        filter[cmp] = True
  for cur in range(len(changes)):
    if filter[cur]: continue
    out.append(changes[cur])
  return out

def Main(args):
  filenames = ParseOptions(args)
  if not filenames:
    gendir = os.path.join(GetOption('gen'), '*.h')
    filenames = sorted(glob.glob(gendir))
    srcdir = os.path.join(GetOption('src'), '*.h')
    srcs = sorted(glob.glob(srcdir))
    for name in srcs:
      name = os.path.split(name)[1]
      name = os.path.join(GetOption('gen'), name)
      if name not in filenames:
        print('Missing: %s' % name)

  for filename in filenames:
    gen = filename
    filename = filename[len(GetOption('gen')) + 1:]
    src = os.path.join(GetOption('src'), filename)
    diff = os.path.join(GetOption('diff'), filename)
    p = subprocess.Popen(['diff', src, gen], stdout=subprocess.PIPE)
    output, errors = p.communicate()

    try:
      input = open(diff, 'rt').read()
    except:
      input = ''

    if input != output:
      changes = GetChanges(output)
    else:
      changes = []

    if changes:
      print("\n\nDelta between:\n  src=%s\n  gen=%s\n" % (src, gen))
      for change in changes:
        change.Dump()
      print('Done with %s\n\n' % src)
      if GetOption('ok'):
        open(diff, 'wt').write(output)
      if GetOption('halt'):
        return 1
    else:
      print("\nSAME:\n  src=%s\n  gen=%s" % (src, gen))
      if input:
        print('  ** Matched expected diff. **')
      print('\n')


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
