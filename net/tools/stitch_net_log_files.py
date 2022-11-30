#!/usr/bin/env python
# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''
This script "stitches" the NetLog files from a ".inprogress" directory to
create a single NetLog file.
'''

import glob
import os
import re
import sys


USAGE ='''Usage: stitch_net_log_files.py <INPROGRESS_DIR> [<OUTPUT_PATH>]

Will copy all the files in <INPROGRESS_DIR> and write the their content into a
NetLog file at path <OUTPUT_PATH>.

If <OUTPUT_PATH> is not specified, it should end with ".inprogress", and the
completed NetLog file will be written to the location with ".inprogress"
stripped.
'''


def get_event_file_sort_key(path):
  '''Returns a tuple (modification timestamp, file number) for a path of the
  form event_file_%d.json'''

  m = re.match('^event_file_(\d+).json$', path)
  file_index = int(m.group(1))
  return (os.path.getmtime(path), file_index)


def get_ordered_event_files():
  '''Returns a list of file paths to event files. The order of the files is
  from oldest to newest. If modification times are the same, files will be
  ordered based on the numeral in their file name.'''

  paths = glob.glob("event_file_*.json")
  paths = sorted(paths, key=get_event_file_sort_key)
  sys.stdout.write("Identified %d event files:\n  %s\n" %
                   (len(paths), "\n  ".join(paths)))
  return paths


def main():
  if len(sys.argv) != 2 and len(sys.argv) != 3:
    sys.stderr.write(USAGE)
    sys.exit(1)

  inprogress_dir = sys.argv[1]
  output_path = None

  # Pick an output path based on command line arguments.
  if len(sys.argv) == 3:
    output_path = sys.argv[2]
  elif len(sys.argv) == 2:
    m = re.match("^(.*)\.inprogress/?$", inprogress_dir)
    if not m:
      sys.stdout.write("Must specify OUTPUT_PATH\n")
      sys.exit(1)
    output_path = m.group(1)

  output_path = os.path.abspath(output_path)

  sys.stdout.write("Reading data from: %s\n" % inprogress_dir)
  sys.stdout.write("Writing log file to: %s\n" % output_path)

  os.chdir(inprogress_dir)

  with open(output_path, "w") as stitched_file:
    try:
      file = open("constants.json")
      with file:
        for line in file:
          stitched_file.write(line)
    except IOError:
      sys.stderr.write("Failed reading \"constants.json\".\n")
      sys.exit(1)

    events_written = False;
    for event_file_path in get_ordered_event_files():
      try:
        file = open(event_file_path)
        with file:
          if not events_written:
            line = file.readline();
            events_written = True
          for next_line in file:
            if next_line.strip() == "":
              line += next_line
            else:
              stitched_file.write(line)
              line = next_line
      except IOError:
        sys.stderr.write("Failed reading \"%s\"\n" % event_file_path)
        sys.exit(1)
    # Remove hanging comma from last event
    # TODO(dconnol): Check if the last line is a valid JSON object. If not,
    # do not write the line to file. This handles incomplete logs.
    line = line.strip()
    if line[-1:] == ",":
      stitched_file.write(line[:-1])
    elif line:
      raise ValueError('Last event is not properly formed')

    if os.path.exists("end_netlog.json"):
      try:
        file = open("end_netlog.json")
        with file:
          for line in file:
            stitched_file.write(line)
      except IOError:
          sys.stderr.write("Failed reading \"end_netlog.json\".\n")
          sys.exit(1)
    else:
      # end_netlog.json won't exist when using this tool to stitch logging
      # sessions that didn't shutdown gracefully.
      #
      # Close the events array and then the log (no polled_data).
      stitched_file.write("]}\n")


if __name__ == "__main__":
  main()
