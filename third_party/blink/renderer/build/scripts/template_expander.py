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

import os
import sys

import jinja2


def apply_template(template_path,
                   params,
                   filters=None,
                   tests=None,
                   template_cache=None):
    template = None

    if filters is None and tests is None and template_cache is not None:
        template = template_cache.get(template_path, None)

    if template is None:
        current_dir = os.path.dirname(os.path.realpath(__file__))
        jinja_env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(current_dir),
            keep_trailing_newline=True,  # newline-terminate generated files
            lstrip_blocks=True,  # so can indent control flow tags
            trim_blocks=True)  # so don't need {%- -%} everywhere
        if filters:
            jinja_env.filters.update(filters)
        if tests:
            jinja_env.tests.update(tests)

        template = jinja_env.get_template(template_path)
        if filters is None and tests is None and template_cache is not None:
            template_cache[template_path] = template

    params['template_file'] = template_path
    return template.render(params)


def use_jinja(template_path, filters=None, tests=None, template_cache=None):
    def real_decorator(generator):
        def generator_internal(*args, **kwargs):
            parameters = generator(*args, **kwargs)
            return apply_template(
                template_path,
                parameters,
                filters=filters,
                tests=tests,
                template_cache=template_cache)

        generator_internal.__name__ = generator.__name__
        return generator_internal

    return real_decorator
