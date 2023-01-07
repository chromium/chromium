#!/usr/bin/env python
# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This file contains helpers for representing, manipulating, and writing
OpenSSL configuration files [1]

Configuration files are simply a collection of name=value "properties", which
are grouped into "sections".

[1] https://www.openssl.org/docs/manmaster/apps/config.html
"""

class Property(object):
  """Represents a key/value pair in OpenSSL .cnf files.

  Names and values are not quoted in any way, so callers need to pass the text
  exactly as it should be written to the file (leading and trailing whitespace
  doesn't matter).

  For instance:
      baseConstraints = critical, CA:false

  Could be represented by a Property where:
    name = 'baseConstraints'
    value = 'critical, CA:false'
  """
  def __init__(self, name, value):
    self.name = name
    self.value = value


  def write_to(self, out):
    """Outputs this property to .cnf file."""
    out.write('%s = %s\n' % (self.name, self.value))


class Section(object):
  """Represents a section in OpenSSL. For instance:
      [CA_root]
      preserve = true

  Could be represented by a Section where:
    name = 'CA_root'
    properties = [Property('preserve', 'true')]
  """
  def __init__(self, name):
    self.name = name
    self.properties = []


  def ensure_property_name_not_duplicated(self, name):
    """Raises an exception of there is more than 1 property named |name|."""
    count = 0
    for prop in self.properties:
      if prop.name == name:
        count += 1
    if count > 1:
      raise Exception('Duplicate property: %s' % (name))


  def set_property(self, name, value):
    """Replaces, adds, or removes a Property from the Section:

      - If |value| is None, then this is equivalent to calling
        remove_property(name)
      - If there is an existing property matching |name| then its value is
        replaced with |value|
      - If there are no properties matching |name| then a new one is added at
        the end of the section

    It is expected that there is AT MOST 1 property with the given name. If
    that is not the case then this function will raise an error."""

    if value is None:
      self.remove_property(name)
      return

    self.ensure_property_name_not_duplicated(name)

    for prop in self.properties:
      if prop.name == name:
        prop.value = value
        return

    self.add_property(name, value)


  def add_property(self, name, value):
    """Adds a property (allows duplicates)"""
    self.properties.append(Property(name, value))


  def remove_property(self, name):
    """Removes the property with the indicated name, if it exists.

    It is expected that there is AT MOST 1 property with the given name. If
    that is not the case then this function will raise an error."""
    self.ensure_property_name_not_duplicated(name)

    for i in range(len(self.properties)):
      if self.properties[i].name == name:
        self.properties.pop(i)
        return


  def clear_properties(self):
    """Removes all configured properties."""
    self.properties = []


  def write_to(self, out):
    """Outputs the section in the format used by .cnf files"""
    out.write('[%s]\n' % (self.name))
    for prop in self.properties:
      prop.write_to(out)
    out.write('\n')


class Config(object):
  """Represents a .cnf (configuration) file in OpenSSL"""
  def __init__(self):
    self.sections = []


  def get_section(self, name):
    """Gets or creates a section with the given name."""
    for section in self.sections:
      if section.name == name:
        return section
    new_section = Section(name)
    self.sections.append(new_section)
    return new_section


  def write_to_file(self, path):
    """Outputs the Config to a .cnf files."""
    with open(path, 'w') as out:
      for section in self.sections:
        section.write_to(out)
