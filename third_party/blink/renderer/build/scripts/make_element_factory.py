#!/usr/bin/env python
# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

from collections import defaultdict

import json5_generator
import template_expander

from make_qualified_names import MakeQualifiedNamesWriter


class MakeElementFactoryWriter(MakeQualifiedNamesWriter):
    default_parameters = {
        'JSInterfaceName': {},
        'Conditional': {},
        'constructorNeedsCreateElementFlags': {},
        'interfaceHeaderDir': {},
        'interfaceName': {},
        'noConstructor': {},
        'noTypeHelpers': {},
        'runtimeEnabled': {},
        'runtimeFlagHasOriginTrial': {},
    }
    default_metadata = dict(
        MakeQualifiedNamesWriter.default_metadata, **{
            'fallbackInterfaceName': '',
            'fallbackJSInterfaceName': '',
        })
    filters = MakeQualifiedNamesWriter.filters

    def __init__(self, json5_file_paths, output_dir, generate_tag_enum=False):
        super(MakeElementFactoryWriter, self).__init__(json5_file_paths,
                                                       output_dir)

        basename = self.namespace.lower() + '_element_factory'
        self._outputs.update({
            (basename + '.h'):
            self.generate_factory_header,
            (basename + '.cc'):
            self.generate_factory_implementation,
        })

        fallback_interface = self.tags_json5_file.metadata[
            'fallbackInterfaceName'].strip('"')
        fallback_js_interface = self.tags_json5_file.metadata[
            'fallbackJSInterfaceName'].strip('"') or fallback_interface

        interface_counts = defaultdict(int)
        tags = self._template_context['tags']
        for tag in tags:
            tag['has_js_interface'] = self._has_js_interface(tag)
            tag['js_interface'] = self._js_interface(tag)
            tag['interface'] = self._interface(tag)
            tag['interface_header'] = '%s/%s.h' % (self._interface_header_dir(
                tag), self.get_file_basename(tag['interface']))
            interface_counts[tag['interface']] += 1

        for tag in tags:
            tag['multipleTagNames'] = (interface_counts[tag['interface']] > 1
                                       or
                                       tag['interface'] == fallback_interface)

        self._template_context.update({
            'fallback_interface':
            fallback_interface,
            'fallback_interface_header':
            self.get_file_basename(fallback_interface) + '.h',
            'fallback_js_interface':
            fallback_js_interface,
            'input_files':
            self._input_files,
            'generate_tag_enum':
            generate_tag_enum
        })

    @template_expander.use_jinja(
        'templates/element_factory.h.tmpl', filters=filters)
    def generate_factory_header(self):
        return self._template_context

    @template_expander.use_jinja(
        'templates/element_factory.cc.tmpl', filters=filters)
    def generate_factory_implementation(self):
        return self._template_context

    def _interface(self, tag):
        if tag['interfaceName']:
            return tag['interfaceName']
        name = tag['name'].to_upper_camel_case()
        # FIXME: We shouldn't hard-code HTML here.
        if name == 'HTML':
            name = 'Html'
        return '%s%sElement' % (self.namespace, name)

    def _js_interface(self, tag):
        if tag['JSInterfaceName']:
            return tag['JSInterfaceName']
        return self._interface(tag)

    def _has_js_interface(self, tag):
        return not tag['noConstructor'] and self._js_interface(tag) != (
            '%sElement' % self.namespace)

    def _interface_header_dir(self, tag):
        if tag['interfaceHeaderDir']:
            return tag['interfaceHeaderDir']
        return 'third_party/blink/renderer/core/' + self.namespace.lower()


if __name__ == "__main__":
    json5_generator.Maker(MakeElementFactoryWriter).main()
