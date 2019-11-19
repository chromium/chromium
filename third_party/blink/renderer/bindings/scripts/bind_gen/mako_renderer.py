# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

import mako.lookup
import mako.template


class MakoRenderer(object):
    """Represents a renderer object implemented with Mako templates."""

    def __init__(self, template_dirs=None):
        self._template_params = {
            "strict_undefined": True,
        }

        self._template_lookup = mako.lookup.TemplateLookup(
            directories=template_dirs, **self._template_params)

        self._caller_stack = []
        self._caller_stack_on_error = []

    def render(self,
               caller,
               template_path=None,
               template_text=None,
               template_vars=None):
        """
        Renders the template with variable bindings.

        It's okay to invoke |render| method recursively and |caller| is pushed
        onto the call stack, which is accessible via |callers| and |last_caller|
        methods.

        Args:
            template_path: A filepath to a template file.
            template_text: A text content to be used as a template.  Either of
                |template_path| or |template_text| must be specified.
            template_vars: Template variable bindings.
            caller: An object to be pushed onto the call stack.
        """

        assert template_path is not None or template_text is not None
        assert template_path is None or template_text is None
        assert isinstance(template_vars, dict)
        assert caller is not None

        self._caller_stack.append(caller)

        try:
            if template_path is not None:
                template = self._template_lookup.get_template(template_path)
            elif template_text is not None:
                template = mako.template.Template(
                    text=template_text, **self._template_params)

            text = template.render(**template_vars)
        except:
            # Print stacktrace of template rendering.
            sys.stderr.write("\n")
            sys.stderr.write("==== template rendering error ====\n")
            sys.stderr.write("  * name: {}, type: {}\n".format(
                _guess_caller_name(self.last_caller), type(self.last_caller)))
            sys.stderr.write("  * depth: {}, module_id: {}\n".format(
                len(self._caller_stack), template.module_id))
            sys.stderr.write("---- template source ----\n")
            sys.stderr.write(template.source)

            # Save the error state at the deepest call.
            current = self._caller_stack
            on_error = self._caller_stack_on_error
            if (len(current) <= len(on_error)
                    and all(current[i] == on_error[i]
                            for i in xrange(len(current)))):
                pass  # Error happened in a deeper caller.
            else:
                self._caller_stack_on_error = list(self._caller_stack)

            raise
        finally:
            self._caller_stack.pop()

        return text

    @property
    def callers(self):
        """
        Returns the callers of this renderer in the order from the last caller
        to the first caller.
        """
        return reversed(self._caller_stack)

    @property
    def last_caller(self):
        """Returns the last caller in the call stack of this renderer."""
        return self._caller_stack[-1]

    @property
    def callers_on_error(self):
        """
        Returns the callers of this renderer in the order from the last caller
        to the first caller at the moment when an exception was thrown.
        """
        return reversed(self._caller_stack_on_error)

    @property
    def last_caller_on_error(self):
        """
        Returns the deepest caller at the moment when an exception was thrown.
        """
        return self._caller_stack_on_error[-1]


def _guess_caller_name(caller):
    """Returns the best-guessed name of |caller|."""
    try:
        # Outer CodeNode may have a binding to the caller.
        for name, value in caller.outer.template_vars.iteritems():
            if value is caller:
                return name
        try:
            # Outer SequenceNode may contain the caller.
            for index, value in enumerate(caller.outer, 1):
                if value is caller:
                    return "{}-of-{}-in-seq".format(index, len(caller.outer))
        except:
            pass
        return "<no name>"
    except:
        return "<unknown>"
