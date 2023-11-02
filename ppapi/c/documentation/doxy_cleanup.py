#!/usr/bin/env python

# Copyright 2011 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''This utility cleans up the html files as emitted by doxygen so
that they are suitable for publication on a Google documentation site.
'''

from __future__ import print_function

import optparse
import os
import re
import shutil
import string
import sys
try:
  from BeautifulSoup import BeautifulSoup, Tag
except (ImportError, NotImplementedError):
  print("This tool requires the BeautifulSoup package "
        "(see http://www.crummy.com/software/BeautifulSoup/).\n"
        "Make sure that the file BeautifulSoup.py is either in this directory "
        "or is available in your PYTHON_PATH")
  raise


class HTMLFixer(object):
  '''This class cleans up the html strings as produced by Doxygen
  '''

  def __init__(self, html):
    self.soup = BeautifulSoup(html)

  def FixTableHeadings(self):
    '''Fixes the doxygen table headings.

    This includes:
      - Using bare <h2> title row instead of row embedded in <tr><td> in table
      - Putting the "name" attribute into the "id" attribute of the <tr> tag.
      - Splitting up tables into multiple separate tables if a table
        heading appears in the middle of a table.

    For example, this html:
     <table>
      <tr><td colspan="2"><h2><a name="pub-attribs"></a>
      Data Fields List</h2></td></tr>
      ...
     </table>

    would be converted to this:
     <h2>Data Fields List</h2>
     <table>
      ...
     </table>
    '''

    table_headers = []
    for tag in self.soup.findAll('tr'):
      if tag.td and tag.td.h2 and tag.td.h2.a and tag.td.h2.a['name']:
        #tag['id'] = tag.td.h2.a['name']
        tag.string = tag.td.h2.a.next
        tag.name = 'h2'
        table_headers.append(tag)

    # reverse the list so that earlier tags don't delete later tags
    table_headers.reverse()
    # Split up tables that have multiple table header (th) rows
    for tag in table_headers:
      print("Header tag: %s is %s" % (tag.name, tag.string.strip()))
      # Is this a heading in the middle of a table?
      if tag.findPreviousSibling('tr') and tag.parent.name == 'table':
        print("Splitting Table named %s" % tag.string.strip())
        table = tag.parent
        table_parent = table.parent
        table_index = table_parent.contents.index(table)
        new_table = Tag(self.soup, name='table', attrs=table.attrs)
        table_parent.insert(table_index + 1, new_table)
        tag_index = table.contents.index(tag)
        for index, row in enumerate(table.contents[tag_index:]):
          new_table.insert(index, row)
      # Now move the <h2> tag to be in front of the <table> tag
      assert tag.parent.name == 'table'
      table = tag.parent
      table_parent = table.parent
      table_index = table_parent.contents.index(table)
      table_parent.insert(table_index, tag)

  def RemoveTopHeadings(self):
    '''Removes <div> sections with a header, tabs, or navpath class attribute'''
    header_tags = self.soup.findAll(
        name='div',
        attrs={'class' : re.compile('^(header|tabs[0-9]*|navpath)$')})
    [tag.extract() for tag in header_tags]

  def FixAll(self):
    self.FixTableHeadings()
    self.RemoveTopHeadings()

  def __str__(self):
    return str(self.soup)


def main():
  '''Main entry for the doxy_cleanup utility

  doxy_cleanup takes a list of html files and modifies them in place.'''

  parser = optparse.OptionParser(usage='Usage: %prog [options] files...')

  parser.add_option('-m', '--move', dest='move', action='store_true',
                    default=False, help='move html files to "original_html"')

  options, files = parser.parse_args()

  if not files:
    parser.print_usage()
    return 1

  for filename in files:
    try:
      with open(filename, 'r') as file:
        html = file.read()

      print("Processing %s" % filename)
      fixer = HTMLFixer(html)
      fixer.FixAll()
      with open(filename, 'w') as file:
        file.write(str(fixer))
      if options.move:
        new_directory = os.path.join(
            os.path.dirname(os.path.dirname(filename)), 'original_html')
        if not os.path.exists(new_directory):
          os.mkdir(new_directory)
        shutil.move(filename, new_directory)
    except:
      print("Error while processing %s" % filename)
      raise

  return 0

if __name__ == '__main__':
  sys.exit(main())
