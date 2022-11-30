# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Based on third_party/WebKit/Source/build/scripts/template_expander.py.

import os.path
import sys

from mojom import fileutil

fileutil.AddLocalRepoThirdPartyDirToModulePath()
import jinja2


def ApplyTemplate(mojo_generator, path_to_template, params, **kwargs):
  loader = jinja2.ModuleLoader(
      os.path.join(mojo_generator.bytecode_path,
                   "%s.zip" % mojo_generator.GetTemplatePrefix()))
  final_kwargs = dict(mojo_generator.GetJinjaParameters())
  final_kwargs.update(kwargs)

  jinja_env = jinja2.Environment(
      loader=loader, keep_trailing_newline=True, **final_kwargs)
  jinja_env.globals.update(mojo_generator.GetGlobals())
  jinja_env.filters.update(mojo_generator.GetFilters())
  template = jinja_env.get_template(path_to_template)
  return template.render(params)


def UseJinja(path_to_template, **kwargs):
  def RealDecorator(generator):
    def GeneratorInternal(*args, **kwargs2):
      parameters = generator(*args, **kwargs2)
      return ApplyTemplate(args[0], path_to_template, parameters, **kwargs)

    GeneratorInternal.__name__ = generator.__name__
    return GeneratorInternal

  return RealDecorator


def ApplyImportedTemplate(mojo_generator, path_to_template, filename, params,
                          **kwargs):
  loader = jinja2.FileSystemLoader(searchpath=path_to_template)
  final_kwargs = dict(mojo_generator.GetJinjaParameters())
  final_kwargs.update(kwargs)

  jinja_env = jinja2.Environment(
      loader=loader, keep_trailing_newline=True, **final_kwargs)
  jinja_env.globals.update(mojo_generator.GetGlobals())
  jinja_env.filters.update(mojo_generator.GetFilters())
  template = jinja_env.get_template(filename)
  return template.render(params)


def UseJinjaForImportedTemplate(func):
  def wrapper(*args, **kwargs):
    parameters = func(*args, **kwargs)
    path_to_template = args[1]
    filename = args[2]
    return ApplyImportedTemplate(args[0], path_to_template, filename,
                                 parameters)

  wrapper.__name__ = func.__name__
  return wrapper


def PrecompileTemplates(generator_modules, output_dir):
  for module in generator_modules.values():
    generator = module.Generator(None)
    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader([
            os.path.join(
                os.path.dirname(module.__file__), generator.GetTemplatePrefix())
        ]))
    jinja_env.filters.update(generator.GetFilters())
    jinja_env.compile_templates(os.path.join(
        output_dir, "%s.zip" % generator.GetTemplatePrefix()),
                                extensions=["tmpl"],
                                zip="stored",
                                ignore_errors=False)
