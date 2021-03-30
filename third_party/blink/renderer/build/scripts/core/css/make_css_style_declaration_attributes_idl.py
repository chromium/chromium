#!/usr/bin/env python

import sys

import json5_generator
import template_expander

from core.css import css_properties

OUT_IDL = 'css_style_declaration_attributes.idl'


class Attribute(object):
    def __init__(self, name, prop):
        super(Attribute, self).__init__()
        self.name = name
        self.prop = prop
        self.runtime_flag = prop['runtime_flag']

    def __str__(self):
        if self.name == 'float':
            return '_float'
        return self.name

    def __eq__(self, o):
        return self.name == o.name

    def __ne__(self, o):
        return self.name != o.name

    def __hash__(self):
        return self.name.__hash__()

    # All versions of the attribute are ImplementedAs the camel cased version.
    def implemented_as(self):
        return css_property_to_idl_attribute(self.prop)


# https://drafts.csswg.org/cssom/#css-property-to-idl-attribute
def css_property_to_idl_attribute(prop, lowercase_first=False):
    name = prop['name'].original
    output = ''
    uppercase_next = False
    if lowercase_first:
        name = name[1:]
    for c in name:
        if c == '-':
            uppercase_next = True
            continue
        if uppercase_next:
            output += c.upper()
            uppercase_next = False
            continue
        output += c
    return Attribute(output, prop)


# https://drafts.csswg.org/cssom/#dom-cssstyledeclaration-camel-cased-attribute
def get_camel_cased_attributes(properties):
    return [css_property_to_idl_attribute(prop) for prop in properties]


# https://drafts.csswg.org/cssom/#dom-cssstyledeclaration-webkit-cased-attribute
def get_webkit_cased_attributes(properties):
    is_webkit_prefixed = lambda x: x['name'].original.startswith('-webkit-')
    properties = filter(is_webkit_prefixed, properties)
    return [css_property_to_idl_attribute(prop, True) for prop in properties]


# https://drafts.csswg.org/cssom/#dom-cssstyledeclaration-dashed-attribute
def get_dashed_attributes(properties):
    has_dash = lambda x: x['name'].original.find('-') != -1
    properties = filter(has_dash, properties)
    return [Attribute(prop['name'].original, prop) for prop in properties]


def get_attributes(properties):
    attributes = get_camel_cased_attributes(properties) + \
        get_webkit_cased_attributes(properties) + \
        get_dashed_attributes(properties)
    assert len(set(attributes)) == len(attributes), \
        'There should be no overlap between the attribute categories'
    return attributes


class CSSStyleDeclarationAttributeIDLWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super(CSSStyleDeclarationAttributeIDLWriter, self).__init__([],
                                                                    output_dir)
        assert len(json5_file_paths) == 3,\
            ('CSSStyleDeclarationAttributeIDLWriter requires 3 input json5 ' +
             'files, got {}.'.format(len(json5_file_paths)))

        properties = css_properties.CSSProperties(json5_file_paths)
        self._attributes = get_attributes(properties.supported_properties)
        self._outputs = {}
        self._outputs[OUT_IDL] = self.generate_attributes_idl

    @template_expander.use_jinja('core/css/templates/%s.tmpl' % OUT_IDL)
    def generate_attributes_idl(self):
        return {'attributes': self._attributes}


if __name__ == '__main__':
    json5_generator.Maker(CSSStyleDeclarationAttributeIDLWriter).main()
