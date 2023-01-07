# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections

import easy_template

def CmpByName(x, y):
  return cmp(x['NAME'], y['NAME'])
  
class LandingPage(object):
  def __init__(self):
    self.section_list = ['Getting Started', 'API', 'Demo', 'Tutorial']
    self.section_map = collections.defaultdict(list)

  def GeneratePage(self, template_path):
    with open(template_path) as template_file:
      template = template_file.read()

    sec_map = {}
    for section_name in self.section_map:
      items = self.section_map[section_name]
      items = sorted(items, cmp=CmpByName)
      sec_map[section_name] = items

    template_dict = { 'section_map': sec_map }
    return easy_template.RunTemplateString(template, template_dict)

  def AddDesc(self, desc):
    group = desc['GROUP']
    assert group in self.section_list
    self.section_map[group].append(desc)
