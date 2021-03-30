#!/usr/bin/env python

import sys

import json5_generator
import template_expander

from core.css import css_properties

OUT_H = 'css_style_declaration_attributes.h'
OUT_CC = 'css_style_declaration_attributes.cc'


class CSSStyleDeclarationAttributeWriter(json5_generator.Writer):
    def __init__(self, json5_file_paths, output_dir):
        super(CSSStyleDeclarationAttributeWriter, self).__init__([],
                                                                 output_dir)
        assert len(json5_file_paths) == 3,\
            ('CSSStyleDeclarationAttributeWriter requires 3 input json5 ' +
             'files, got {}.'.format(len(json5_file_paths)))

        properties = css_properties.CSSProperties(
            json5_file_paths).supported_properties
        self._properties = list(filter(lambda p: p['runtime_flag'],
                                       properties))
        self._outputs = {}
        self._outputs[OUT_H] = self.generate_attributes_h
        self._outputs[OUT_CC] = self.generate_attributes_cc

    @template_expander.use_jinja('core/css/templates/%s.tmpl' % OUT_H)
    def generate_attributes_h(self):
        return {'properties': self._properties}

    @template_expander.use_jinja('core/css/templates/%s.tmpl' % OUT_CC)
    def generate_attributes_cc(self):
        return {'properties': self._properties}


if __name__ == '__main__':
    json5_generator.Maker(CSSStyleDeclarationAttributeWriter).main()
