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

from __future__ import print_function

import os.path
import sys

import json5_generator
import license
import name_utilities
import template_expander


# All events on the following list are matched case-insensitively
# in createEvent.
#
# https://dom.spec.whatwg.org/#dom-document-createevent
def create_event_ignore_case_list(name):
    return (name == 'HTMLEvents' or name == 'Event' or name == 'Events'
            or name.startswith('UIEvent') or name.startswith('CustomEvent')
            or name == 'KeyboardEvent' or name == 'MessageEvent'
            or name.startswith('MouseEvent') or name == 'TouchEvent')


# All events on the following list are matched case-insensitively in createEvent
# and are measured using UseCounter.
#
# TODO(https://crbug.com/569690): All events on this list should either be added
# to the spec and moved to the above list (causing them to be matched
# case-insensitively) or be deprecated/removed.
def create_event_ignore_case_and_measure_list(name):
    return (name == 'AnimationEvent' or name == 'BeforeUnloadEvent'
            or name == 'CloseEvent' or name == 'CompositionEvent'
            or name == 'DeviceMotionEvent' or name == 'DeviceOrientationEvent'
            or name == 'DragEvent' or name == 'ErrorEvent'
            or name == 'FocusEvent' or name == 'HashChangeEvent'
            or name == 'IDBVersionChangeEvent' or name == 'KeyboardEvents'
            or name == 'MutationEvent' or name == 'MutationEvents'
            or name == 'PageTransitionEvent' or name == 'PopStateEvent'
            or name == 'StorageEvent' or name == 'SVGEvents'
            or name == 'TextEvent' or name == 'TrackEvent'
            or name == 'TransitionEvent' or name == 'WebGLContextEvent'
            or name == 'WheelEvent')


def measure_name(name):
    return 'DocumentCreateEvent' + name


class EventFactoryWriter(json5_generator.Writer):
    default_parameters = {
        'ImplementedAs': {},
        'interfaceHeaderDir': {},
        'RuntimeEnabled': {},
    }
    default_metadata = {
        'export': '',
        'namespace': '',
        'suffix': '',
    }
    filters = {
        'cpp_name': name_utilities.cpp_name,
        'name': lambda entry: entry['name'].original,
        'create_event_ignore_case_list': create_event_ignore_case_list,
        'measure_name': measure_name,
    }

    def __init__(self, json5_file_path, output_dir):
        super(EventFactoryWriter, self).__init__(json5_file_path, output_dir)
        self.namespace = self.json5_file.metadata['namespace'].strip('"')
        assert self.namespace == 'event_interface_names', \
            'namespace field should be "event_interface_names".'
        self.suffix = self.json5_file.metadata['suffix'].strip('"')
        snake_suffix = (self.suffix.lower() + '_') if self.suffix else ''
        self._outputs = {
            ('event_%sfactory.cc' % snake_suffix):
            self.generate_implementation,
        }

    def _fatal(self, message):
        print('FATAL ERROR: ' + message)
        exit(1)

    def _headers_header_include_path(self, entry):
        path = entry['interfaceHeaderDir']
        if not path:
            return None
        return path + '/' + self.get_file_basename(
            name_utilities.cpp_name(entry)) + '.h'

    def _headers_header_includes(self, entries):
        includes = {
            'third_party/blink/renderer/core/execution_context/execution_context.h',
            'third_party/blink/renderer/core/frame/deprecation.h',
            'third_party/blink/renderer/platform/instrumentation/use_counter.h',
            'third_party/blink/renderer/platform/runtime_enabled_features.h',
        }
        includes.update(map(self._headers_header_include_path, entries))
        return sorted([x for x in includes if x])

    @template_expander.use_jinja(
        'templates/event_factory.cc.tmpl', filters=filters)
    def generate_implementation(self):
        target_events = [
            event for event in self.json5_file.name_dictionaries if
            (create_event_ignore_case_list(event['name'].original) or
             create_event_ignore_case_and_measure_list(event['name'].original))
        ]
        return {
            'include_header_paths':
            self._headers_header_includes(target_events),
            'input_files': self._input_files,
            'suffix': self.suffix,
            'events': target_events,
        }


if __name__ == "__main__":
    json5_generator.Maker(EventFactoryWriter).main()
