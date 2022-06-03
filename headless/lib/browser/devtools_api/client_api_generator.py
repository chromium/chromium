# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import collections
import functools
import os.path
import re
import sys

try:
  import json
except ImportError:
  import simplejson as json

# Path handling for libraries and templates
# Paths have to be normalized because Jinja uses the exact template path to
# determine the hash used in the cache filename, and we need a pre-caching step
# to be concurrency-safe. Use absolute path because __file__ is absolute if
# module is imported, and relative if executed directly.
# If paths differ between pre-caching and individual file compilation, the cache
# is regenerated, which causes a race condition and breaks concurrent build,
# since some compile processes will try to read the partially written cache.
module_path, module_filename = os.path.split(os.path.realpath(__file__))
third_party_dir = os.path.normpath(
    os.path.join(module_path, os.pardir, os.pardir, os.pardir, os.pardir,
                 'third_party'))
templates_dir = module_path

# jinja2 is in chromium's third_party directory.
# Insert at 1 so at front to override system libraries, and
# after path[0] == invoking script dir
sys.path.insert(1, third_party_dir)
import jinja2


def ParseArguments(args):
  """Parses command line arguments and returns a (json_api, output_dir) tuple.
  """
  cmdline_parser = argparse.ArgumentParser()
  cmdline_parser.add_argument('--protocol', required=True)
  cmdline_parser.add_argument('--output_dir', required=True)

  args = cmdline_parser.parse_args(args)
  with open(args.protocol, 'r') as f:
    return json.load(f), args.output_dir


def ToTitleCase(name):
  return name[:1].upper() + name[1:]


def DashToCamelCase(word):
  return ''.join(ToTitleCase(x) for x in word.split('-'))


def CamelCaseToHackerStyle(name):
  # Do two passes to insert '_' chars to deal with overlapping matches (e.g.,
  # 'LoLoLoL').
  name = re.sub(r'([^_])([A-Z][a-z]+?)', r'\1_\2', name)
  name = re.sub(r'([^_])([A-Z][a-z]+?)', r'\1_\2', name)
  return name.lower()


def SanitizeLiteral(literal):
  return {
      # Rename null enumeration values to avoid a clash with the NULL macro.
      'null': 'none',
      # Rename literals that clash with Win32 defined macros.
      'error': 'err',
      'mouseMoved': 'mouse_ptr_moved',
      'Strict': 'exact',
      'getCurrentTime': 'getCurrentAnimationTime',
      # Rename mathematical constants to avoid colliding with C macros.
      'Infinity': 'InfinityValue',
      '-Infinity': 'NegativeInfinityValue',
      'NaN': 'NaNValue',
      # Turn negative zero into a safe identifier.
      '-0': 'NegativeZeroValue',
  }.get(literal, literal)


def InitializeJinjaEnv(cache_dir):
  jinja_env = jinja2.Environment(
      loader=jinja2.FileSystemLoader(templates_dir),
      # Bytecode cache is not concurrency-safe unless pre-cached:
      # if pre-cached this is read-only, but writing creates a race condition.
      bytecode_cache=jinja2.FileSystemBytecodeCache(cache_dir),
      keep_trailing_newline=True,  # Newline-terminate generated files.
      lstrip_blocks=True,  # So we can indent control flow tags.
      trim_blocks=True)
  jinja_env.filters.update({
      'to_title_case': ToTitleCase,
      'dash_to_camelcase': DashToCamelCase,
      'camelcase_to_hacker_style': CamelCaseToHackerStyle,
      'sanitize_literal': SanitizeLiteral,
  })
  jinja_env.add_extension('jinja2.ext.loopcontrols')
  return jinja_env


def PatchFullQualifiedRefs(json_api):
  def PatchFullQualifiedRefsInDomain(json, domain_name):
    if isinstance(json, list):
      for item in json:
        PatchFullQualifiedRefsInDomain(item, domain_name)

    if not isinstance(json, dict):
      return
    for key in json:
      if key != '$ref':
        PatchFullQualifiedRefsInDomain(json[key], domain_name)
        continue
      if not '.' in json['$ref']:
        json['$ref'] = domain_name + '.' + json['$ref']

  for domain in json_api['domains']:
    PatchFullQualifiedRefsInDomain(domain, domain['domain'])


def CreateUserTypeDefinition(domain, type):
  namespace = CamelCaseToHackerStyle(domain['domain'])
  return {
      'return_type': 'std::unique_ptr<::headless::%s::%s>' % (
          namespace, type['id']),
      'pass_type': 'std::unique_ptr<::headless::%s::%s>' % (
          namespace, type['id']),
      'to_raw_type': '*%s',
      'to_raw_return_type': '%s.get()',
      'to_pass_type': 'std::move(%s)',
      'type': 'std::unique_ptr<::headless::%s::%s>' % (namespace, type['id']),
      'raw_type': '::headless::%s::%s' % (namespace, type['id']),
      'raw_pass_type': '::headless::%s::%s*' % (namespace, type['id']),
      'raw_return_type': 'const ::headless::%s::%s*' % (namespace, type['id']),
  }


def CreateEnumTypeDefinition(domain_name, type):
  namespace = CamelCaseToHackerStyle(domain_name)
  return {
      'return_type': '::headless::%s::%s' % (namespace, type['id']),
      'pass_type': '::headless::%s::%s' % (namespace, type['id']),
      'to_raw_type': '%s',
      'to_raw_return_type': '%s',
      'to_pass_type': '%s',
      'type': '::headless::%s::%s' % (namespace, type['id']),
      'raw_type': '::headless::%s::%s' % (namespace, type['id']),
      'raw_pass_type': '::headless::%s::%s' % (namespace, type['id']),
      'raw_return_type': '::headless::%s::%s' % (namespace, type['id']),
  }


def CreateObjectTypeDefinition():
  return {
      'return_type': 'std::unique_ptr<base::DictionaryValue>',
      'pass_type': 'std::unique_ptr<base::DictionaryValue>',
      'to_raw_type': '*%s',
      'to_raw_return_type': '%s.get()',
      'to_pass_type': 'std::move(%s)',
      'type': 'std::unique_ptr<base::DictionaryValue>',
      'raw_type': 'base::DictionaryValue',
      'raw_pass_type': 'base::DictionaryValue*',
      'raw_return_type': 'const base::DictionaryValue*',
  }


def WrapObjectTypeDefinition(type):
  id = type.get('id', 'base::Value')
  return {
      'return_type': 'std::unique_ptr<%s>' % id,
      'pass_type': 'std::unique_ptr<%s>' % id,
      'to_raw_type': '*%s',
      'to_raw_return_type': '%s.get()',
      'to_pass_type': 'std::move(%s)',
      'type': 'std::unique_ptr<%s>' % id,
      'raw_type': id,
      'raw_pass_type': '%s*' % id,
      'raw_return_type': 'const %s*' % id,
  }


def CreateAnyTypeDefinition():
  return {
      'return_type': 'std::unique_ptr<base::Value>',
      'pass_type': 'std::unique_ptr<base::Value>',
      'to_raw_type': '*%s',
      'to_raw_return_type': '%s.get()',
      'to_pass_type': 'std::move(%s)',
      'type': 'std::unique_ptr<base::Value>',
      'raw_type': 'base::Value',
      'raw_pass_type': 'base::Value*',
      'raw_return_type': 'const base::Value*',
  }


def CreateStringTypeDefinition():
  return {
      'return_type': 'std::string',
      'pass_type': 'const std::string&',
      'to_pass_type': '%s',
      'to_raw_type': '%s',
      'to_raw_return_type': '%s',
      'type': 'std::string',
      'raw_type': 'std::string',
      'raw_pass_type': 'const std::string&',
      'raw_return_type': 'std::string',
  }


def CreateBinaryTypeDefinition():
  return {
      'return_type': 'protocol::Binary',
      'pass_type': 'const protocol::Binary&',
      'to_pass_type': '%s',
      'to_raw_type': '%s',
      'to_raw_return_type': '%s',
      'type': 'protocol::Binary',
      'raw_type': 'protocol::Binary',
      'raw_pass_type': 'const protocol::Binary&',
      'raw_return_type': 'protocol::Binary',
  }


def CreatePrimitiveTypeDefinition(type):
  typedefs = {
      'number': 'double',
      'integer': 'int',
      'boolean': 'bool',
  }
  return {
      'return_type': typedefs[type],
      'pass_type': typedefs[type],
      'to_pass_type': '%s',
      'to_raw_type': '%s',
      'to_raw_return_type': '%s',
      'type': typedefs[type],
      'raw_type': typedefs[type],
      'raw_pass_type': typedefs[type],
      'raw_return_type': typedefs[type],
  }


type_definitions = {}
type_definitions['number'] = CreatePrimitiveTypeDefinition('number')
type_definitions['integer'] = CreatePrimitiveTypeDefinition('integer')
type_definitions['boolean'] = CreatePrimitiveTypeDefinition('boolean')
type_definitions['string'] = CreateStringTypeDefinition()
type_definitions['binary'] = CreateBinaryTypeDefinition()
type_definitions['object'] = CreateObjectTypeDefinition()
type_definitions['any'] = CreateAnyTypeDefinition()


def WrapArrayDefinition(type):
  return {
      'return_type': 'std::vector<%s>' % type['type'],
      'pass_type': 'std::vector<%s>' % type['type'],
      'to_raw_type': '%s',
      'to_raw_return_type': '&%s',
      'to_pass_type': 'std::move(%s)',
      'type': 'std::vector<%s>' % type['type'],
      'raw_type': 'std::vector<%s>' % type['type'],
      'raw_pass_type': 'std::vector<%s>*' % type['type'],
      'raw_return_type': 'const std::vector<%s>*' % type['type'],
  }


def CreateTypeDefinitions(json_api):
  for domain in json_api['domains']:
    if not ('types' in domain):
      continue
    for type in domain['types']:
      if type['type'] == 'object':
        if 'properties' in type:
          type_definitions[domain['domain'] + '.' + type['id']] = (
              CreateUserTypeDefinition(domain, type))
        else:
          type_definitions[domain['domain'] + '.' + type['id']] = (
              CreateObjectTypeDefinition())
      elif type['type'] == 'array':
        type_definitions[domain['domain'] + '.' + type['id']] = (
            ResolveType(type))
      elif 'enum' in type:
        type_definitions[domain['domain'] + '.' + type['id']] = (
            CreateEnumTypeDefinition(domain['domain'], type))
        type['$ref'] = domain['domain'] + '.' + type['id']
      elif type['type'] == 'any':
        type_definitions[domain['domain'] + '.' + type['id']] = (
            CreateAnyTypeDefinition())
      elif type['type'] == 'string':
        type_definitions[domain['domain'] + '.' + type['id']] = (
            CreateStringTypeDefinition())
      elif type['type'] == 'binary':
        type_definitions[domain['domain'] + '.' + type['id']] = (
            CreateBinaryTypeDefinition())
      else:
        type_definitions[domain['domain'] + '.' + type['id']] = (
            CreatePrimitiveTypeDefinition(type['type']))


def TypeDefinition(name):
  return type_definitions[name]


def ResolveType(property):
  if '$ref' in property:
    return type_definitions[property['$ref']]
  elif property['type'] == 'object':
    return WrapObjectTypeDefinition(property)
  elif property['type'] == 'array':
    return WrapArrayDefinition(ResolveType(property['items']))
  return type_definitions[property['type']]


def JoinArrays(dict, keys):
  result = []
  for key in keys:
    if key in dict:
      result += dict[key]
  return result


def SynthesizeEnumType(domain, owner, type):
  type['id'] = ToTitleCase(owner) + ToTitleCase(type['name'])
  type_definitions[domain['domain'] + '.' + type['id']] = (
      CreateEnumTypeDefinition(domain['domain'], type))
  type['$ref'] = domain['domain'] + '.' + type['id']
  domain['types'].append(type)


def SynthesizeCommandTypes(json_api):
  """Generate types for command parameters, return values and enum
     properties.
  """
  for domain in json_api['domains']:
    if not 'types' in domain:
      domain['types'] = []
    for type in domain['types']:
      if type['type'] == 'object':
        for property in type.get('properties', []):
          if 'enum' in property and not '$ref' in property:
            SynthesizeEnumType(domain, type['id'], property)

    for command in domain.get('commands', []):
      parameters_required = False
      if 'parameters' in command:
        for parameter in command['parameters']:
          if not 'optional' in parameter:
            parameters_required = True
          if 'enum' in parameter and not '$ref' in parameter:
            SynthesizeEnumType(domain, command['name'], parameter)
        parameters_type = {
            'id': ToTitleCase(SanitizeLiteral(command['name'])) + 'Params',
            'type': 'object',
            'description': 'Parameters for the %s command.' % ToTitleCase(
                SanitizeLiteral(command['name'])),
            'properties': command['parameters']
        }
        domain['types'].append(parameters_type)
      if 'returns' in command:
        for parameter in command['returns']:
          if 'enum' in parameter and not '$ref' in parameter:
            SynthesizeEnumType(domain, command['name'], parameter)
        result_type = {
            'id': ToTitleCase(SanitizeLiteral(command['name'])) + 'Result',
            'type': 'object',
            'description': 'Result for the %s command.' % ToTitleCase(
                SanitizeLiteral(command['name'])),
            'properties': command['returns']
        }
        domain['types'].append(result_type)
      command['parameters_required'] = parameters_required


def SynthesizeEventTypes(json_api):
  """Generate types for events and their properties.

  Note that parameter objects are also created for events without parameters to
  make it easier to introduce parameters later.
  """
  for domain in json_api['domains']:
    if not 'types' in domain:
      domain['types'] = []
    for event in domain.get('events', []):
      for parameter in event.get('parameters', []):
        if 'enum' in parameter and not '$ref' in parameter:
          SynthesizeEnumType(domain, event['name'], parameter)
      event_type = {
          'id': ToTitleCase(event['name']) + 'Params',
          'type': 'object',
          'description': 'Parameters for the %s event.' % ToTitleCase(
              event['name']),
          'properties': event.get('parameters', [])
      }
      domain['types'].append(event_type)


def InitializeDomainDependencies(json_api):
  """For each domain create list of domains given domain depends on,
  including itself."""

  direct_deps = collections.defaultdict(set)
  types_required = collections.defaultdict(set)

  def GetDomainDepsFromRefs(domain_name, json):
    if isinstance(json, list):
      for value in json:
        GetDomainDepsFromRefs(domain_name, value)
      return

    if not isinstance(json, dict):
      return
    for value in json.values():
      GetDomainDepsFromRefs(domain_name, value)

    if '$ref' in json:
      if '.' in json['$ref']:
        dep = json['$ref'].split('.')[0]
        direct_deps[domain_name].add(dep)
        types_required[domain_name].add(json['$ref'])

  for domain in json_api['domains']:
    direct_deps[domain['domain']] = set(domain.get('dependencies', []))
    types_required[domain['domain']] = set(domain.get('types_required', []))
    GetDomainDepsFromRefs(domain['domain'], domain)

  def TraverseDependencies(domain, deps):
    if domain in deps:
      return
    deps.add(domain)

    for dep in direct_deps[domain]:
      TraverseDependencies(dep, deps)

  for domain in json_api['domains']:
    domain_deps = set()
    TraverseDependencies(domain['domain'], domain_deps)
    domain['dependencies'] = sorted(domain_deps)


def PatchExperimentalCommandsAndEvents(json_api):
  """Mark all commands and events in experimental domains as experimental
  and make sure experimental commands have at least empty parameters
  and return values.
  """
  for domain in json_api['domains']:
    if domain.get('experimental', False):
      for command in domain.get('commands', []):
        command['experimental'] = True
      for event in domain.get('events', []):
        event['experimental'] = True


def EnsureDirectoryExists(path):
  if not os.path.exists(path):
    os.makedirs(path)


def EnsureCommandsHaveParametersAndReturnTypes(json_api):
  """Make sure all commands have at least empty parameters and return values.
  This guarantees API compatibility if a previously experimental command is made
  stable.
  """
  for domain in json_api['domains']:
    for command in domain.get('commands', []):
      if not 'parameters' in command:
        command['parameters'] = []
      if not 'returns' in command:
        command['returns'] = []
    for event in domain.get('events', []):
      if not 'parameters' in event:
        event['parameters'] = []


def GeneratePerDomain(jinja_env, output_dirname, json_api, class_name,
                    file_types, domain_name_to_file_name_func):
  EnsureDirectoryExists(output_dirname)
  for file_type in file_types:
    template = jinja_env.get_template('/%s_%s.template' % (
        class_name, file_type))
    for domain in json_api['domains']:
      template_context = {
          'domain': domain,
          'resolve_type': ResolveType,
      }
      domain_name = CamelCaseToHackerStyle(domain['domain'])
      output_file = '%s/%s.%s' % (output_dirname,
                                  domain_name_to_file_name_func(domain_name),
                                  file_type)
      with open(output_file, 'w') as f:
        f.write(template.render(template_context))


def GenerateDomains(jinja_env, output_dirname, json_api):
  GeneratePerDomain(
      jinja_env, os.path.join(output_dirname, 'devtools', 'domains'), json_api,
      'domain', ['cc', 'h'],
      lambda domain_name: domain_name)


def GenerateTypes(jinja_env, output_dirname, json_api):
  # Generate forward declarations for types.
  GeneratePerDomain(
      jinja_env, os.path.join(output_dirname, 'devtools', 'internal'),
      json_api, 'domain_types_forward_declarations', ['h'],
      lambda domain_name: 'types_forward_declarations_%s' % (domain_name, ))
  # Generate types on per-domain basis.
  GeneratePerDomain(
      jinja_env, os.path.join(output_dirname, 'devtools', 'domains'),
      json_api, 'domain_types', ['h', 'cc'],
      lambda domain_name: 'types_%s' % (domain_name, ))


def GenerateTypeConversions(jinja_env, output_dirname, json_api):
  # Generate type conversions on per-domain basis.
  GeneratePerDomain(
      jinja_env, os.path.join(output_dirname, 'devtools', 'internal'),
      json_api, 'domain_type_conversions', ['h'],
      lambda domain_name: 'type_conversions_%s' % (domain_name, ))


if __name__ == '__main__':
  json_api, output_dirname = ParseArguments(sys.argv[1:])
  jinja_env = InitializeJinjaEnv(output_dirname)
  InitializeDomainDependencies(json_api)
  PatchExperimentalCommandsAndEvents(json_api)
  EnsureCommandsHaveParametersAndReturnTypes(json_api)
  SynthesizeCommandTypes(json_api)
  SynthesizeEventTypes(json_api)
  PatchFullQualifiedRefs(json_api)
  CreateTypeDefinitions(json_api)
  GenerateDomains(jinja_env, output_dirname, json_api)
  GenerateTypes(jinja_env, output_dirname, json_api)
  GenerateTypeConversions(jinja_env, output_dirname, json_api)
