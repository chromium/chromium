#!/usr/bin/env python3

# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import client_api_generator
import shutil
import sys
import tempfile
import unittest


class ClientApiGeneratorTest(unittest.TestCase):

  def test_ArgumentParsing(self):
    with tempfile.NamedTemporaryFile() as f:
      f.write(b'{"foo": true}')
      f.flush()
      json_api, output_dir = client_api_generator.ParseArguments([
          '--protocol', f.name, '--output_dir', 'out'])
      self.assertEqual({'foo': True}, json_api)
      self.assertEqual('out', output_dir)

  def test_ToTitleCase(self):
    self.assertEqual(client_api_generator.ToTitleCase('fooBar'), 'FooBar')

  def test_DashToCamelCase(self):
    self.assertEqual(client_api_generator.DashToCamelCase('foo-bar'), 'FooBar')
    self.assertEqual(client_api_generator.DashToCamelCase('foo-'), 'Foo')
    self.assertEqual(client_api_generator.DashToCamelCase('-bar'), 'Bar')

  def test_CamelCaseToHackerStyle(self):
    self.assertEqual(client_api_generator.CamelCaseToHackerStyle('FooBar'),
                     'foo_bar')
    self.assertEqual(client_api_generator.CamelCaseToHackerStyle('LoLoLoL'),
                     'lo_lo_lol')

  def test_SanitizeLiteralEnum(self):
    self.assertEqual(client_api_generator.SanitizeLiteral('foo'), 'foo')
    self.assertEqual(client_api_generator.SanitizeLiteral('null'), 'none')
    self.assertEqual(client_api_generator.SanitizeLiteral('Infinity'),
                                                          'InfinityValue')

  def test_PatchFullQualifiedRefs(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain0',
          '$ref': 'reference',
        },
        {
          'domain': 'domain1',
          '$ref': 'reference',
          'more': [{'$ref': 'domain0.thing'}],
        }
      ]
    }
    expected_json_api = {
      'domains': [
        {
          'domain': 'domain0',
          '$ref': 'domain0.reference',
        },
        {
          'domain': 'domain1',
          '$ref': 'domain1.reference',
          'more': [{'$ref': 'domain0.thing'}],
        }
      ]
    }
    client_api_generator.PatchFullQualifiedRefs(json_api)
    self.assertDictEqual(json_api, expected_json_api)

  def test_NumberType(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'types': [
            {
              'id': 'TestType',
              'type': 'number',
            },
          ]
        },
      ]
    }
    client_api_generator.CreateTypeDefinitions(json_api)
    type = json_api['domains'][0]['types'][0]
    resolved = client_api_generator.ResolveType(type)
    self.assertEqual('double', resolved['raw_type'])

  def test_IntegerType(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'types': [
            {
              'id': 'TestType',
              'type': 'integer',
            },
          ]
        },
      ]
    }
    client_api_generator.CreateTypeDefinitions(json_api)
    type = json_api['domains'][0]['types'][0]
    resolved = client_api_generator.ResolveType(type)
    self.assertEqual('int', resolved['raw_type'])

  def test_BooleanType(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'types': [
            {
              'id': 'TestType',
              'type': 'boolean',
            },
          ]
        },
      ]
    }
    client_api_generator.CreateTypeDefinitions(json_api)
    type = json_api['domains'][0]['types'][0]
    resolved = client_api_generator.ResolveType(type)
    self.assertEqual('bool', resolved['raw_type'])

  def test_StringType(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'types': [
            {
              'id': 'TestType',
              'type': 'string',
            },
          ]
        },
      ]
    }
    client_api_generator.CreateTypeDefinitions(json_api)
    type = json_api['domains'][0]['types'][0]
    resolved = client_api_generator.ResolveType(type)
    self.assertEqual('std::string', resolved['raw_type'])
    self.assertEqual('const std::string&', resolved['pass_type'])

  def test_ObjectType(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'types': [
            {
              'id': 'TestType',
              'type': 'object',
              'properties': [
                  {'name': 'p1', 'type': 'number'},
                  {'name': 'p2', 'type': 'integer'},
                  {'name': 'p3', 'type': 'boolean'},
                  {'name': 'p4', 'type': 'string'},
                  {'name': 'p5', 'type': 'any'},
                  {'name': 'p6', 'type': 'object', '$ref': 'TestType'},
              ],
              'returns': [
                  {'name': 'r1', 'type': 'number'},
                  {'name': 'r2', 'type': 'integer'},
                  {'name': 'r3', 'type': 'boolean'},
                  {'name': 'r4', 'type': 'string'},
                  {'name': 'r5', 'type': 'any'},
                  {'name': 'r6', 'type': 'object', '$ref': 'TestType'},
              ],
            },
          ]
        },
      ]
    }
    client_api_generator.CreateTypeDefinitions(json_api)
    type = json_api['domains'][0]['types'][0]
    resolved = client_api_generator.ResolveType(type)
    self.assertEqual('TestType', resolved['raw_type'])

  def test_AnyType(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'types': [
            {
              'id': 'TestType',
              'type': 'any',
            },
          ]
        },
      ]
    }
    client_api_generator.CreateTypeDefinitions(json_api)
    type = json_api['domains'][0]['types'][0]
    resolved = client_api_generator.ResolveType(type)
    self.assertEqual('base::Value', resolved['raw_type'])

  def test_ArrayType(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'types': [
            {
              'id': 'TestType',
              'type': 'array',
              'items': {'type': 'integer'}
            },
          ]
        },
      ]
    }
    client_api_generator.CreateTypeDefinitions(json_api)
    type = json_api['domains'][0]['types'][0]
    resolved = client_api_generator.ResolveType(type)
    self.assertEqual('std::vector<int>', resolved['raw_type'])

  def test_EnumType(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'types': [
            {
              'id': 'TestType',
              'type': 'string',
              'enum': ['a', 'b', 'c']
            },
          ]
        },
      ]
    }
    client_api_generator.CreateTypeDefinitions(json_api)
    type = json_api['domains'][0]['types'][0]
    resolved = client_api_generator.ResolveType(type)
    self.assertEqual('::headless::domain::TestType', resolved['raw_type'])

  def test_SynthesizeCommandTypes(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'commands': [
            {
              'name': 'TestCommand',
              'parameters': [
                  {'name': 'p1', 'type': 'number'},
                  {'name': 'p2', 'type': 'integer'},
                  {'name': 'p3', 'type': 'boolean'},
                  {'name': 'p4', 'type': 'string'},
                  {'name': 'p5', 'type': 'any'},
                  {'name': 'p6', 'type': 'object', '$ref': 'TestType'},
              ],
              'returns': [
                  {'name': 'r1', 'type': 'number'},
                  {'name': 'r2', 'type': 'integer'},
                  {'name': 'r3', 'type': 'boolean'},
                  {'name': 'r4', 'type': 'string'},
                  {'name': 'r5', 'type': 'any'},
                  {'name': 'r6', 'type': 'object', '$ref': 'TestType'},
              ],
            },
          ]
        },
      ]
    }
    expected_types = [
      {
        'type': 'object',
        'id': 'TestCommandParams',
        'description': 'Parameters for the TestCommand command.',
        'properties': [
          {'type': 'number', 'name': 'p1'},
          {'type': 'integer', 'name': 'p2'},
          {'type': 'boolean', 'name': 'p3'},
          {'type': 'string', 'name': 'p4'},
          {'type': 'any', 'name': 'p5'},
          {'type': 'object', 'name': 'p6', '$ref': 'TestType'}
        ],
      },
      {
        'type': 'object',
        'id': 'TestCommandResult',
        'description': 'Result for the TestCommand command.',
        'properties': [
          {'type': 'number', 'name': 'r1'},
          {'type': 'integer', 'name': 'r2'},
          {'type': 'boolean', 'name': 'r3'},
          {'type': 'string', 'name': 'r4'},
          {'type': 'any', 'name': 'r5'},
          {'type': 'object', 'name': 'r6', '$ref': 'TestType'}
        ],
      }
    ]
    client_api_generator.SynthesizeCommandTypes(json_api)
    types = json_api['domains'][0]['types']
    self.assertListEqual(types, expected_types)

  def test_SynthesizeEventTypes(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'events': [
            {
              'name': 'TestEvent',
              'parameters': [
                  {'name': 'p1', 'type': 'number'},
                  {'name': 'p2', 'type': 'integer'},
                  {'name': 'p3', 'type': 'boolean'},
                  {'name': 'p4', 'type': 'string'},
                  {'name': 'p5', 'type': 'any'},
                  {'name': 'p6', 'type': 'object', '$ref': 'TestType'},
              ]
            },
            {
              'name': 'TestEventWithNoParams',
            }
          ]
        }
      ]
    }
    expected_types = [
      {
        'type': 'object',
        'id': 'TestEventParams',
        'description': 'Parameters for the TestEvent event.',
        'properties': [
          {'type': 'number', 'name': 'p1'},
          {'type': 'integer', 'name': 'p2'},
          {'type': 'boolean', 'name': 'p3'},
          {'type': 'string', 'name': 'p4'},
          {'type': 'any', 'name': 'p5'},
          {'type': 'object', 'name': 'p6', '$ref': 'TestType'}
        ]
      },
      {
        'type': 'object',
        'id': 'TestEventWithNoParamsParams',
        'description': 'Parameters for the TestEventWithNoParams event.',
        'properties': [],
      }
    ]
    client_api_generator.SynthesizeEventTypes(json_api)
    types = json_api['domains'][0]['types']
    self.assertListEqual(types, expected_types)

  def test_InitializeDomainDependencies(self):
    json_api = {
      'domains': [
        {
          'domain': 'Domain1',
          'types': [
            {
              'id': 'TestType',
              'type': 'object',
              'properties': [
                  {'name': 'p1', 'type': 'object', '$ref': 'Domain2.TestType'},
              ],
            },
          ],
        },
        {
          'domain': 'Domain2',
          'dependencies': ['Domain3'],
          'types': [
            {
              'id': 'TestType',
              'type': 'object',
              'properties': [
                  {'name': 'p1', 'type': 'object', '$ref': 'Domain1.TestType'},
              ],
            },
          ],
        },
        {
          'domain': 'Domain3',
        },
        {
          'domain': 'Domain4',
          'dependencies': ['Domain1'],
        },
      ]
    }
    client_api_generator.InitializeDomainDependencies(json_api)

    dependencies = [ {
      'domain': domain['domain'],
      'dependencies': domain['dependencies']
    } for domain in json_api['domains'] ]

    self.assertListEqual(dependencies, [ {
        "domain": "Domain1",
        "dependencies": ["Domain1", "Domain2", "Domain3"],
      }, {
        "domain": "Domain2",
        "dependencies": ["Domain1", "Domain2", "Domain3"],
      }, {
        "domain": "Domain3",
        "dependencies": ["Domain3"],
      }, {
        "domain": "Domain4",
        "dependencies": ["Domain1", "Domain2", "Domain3", "Domain4"],
      }
    ])

  def test_PatchExperimentalDomains(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'experimental': True,
          'commands': [
            {
              'name': 'FooCommand',
            }
          ],
          'events': [
            {
              'name': 'BarEvent',
            }
          ]
        }
      ]
    }
    client_api_generator.PatchExperimentalCommandsAndEvents(json_api)
    for command in json_api['domains'][0]['commands']:
      self.assertTrue(command['experimental'])
    for event in json_api['domains'][0]['events']:
      self.assertTrue(command['experimental'])

  def test_EnsureCommandsHaveParametersAndReturnTypes(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'commands': [
            {
              'name': 'FooCommand',
            }
          ],
          'events': [
            {
              'name': 'BarEvent',
            }
          ]
        }
      ]
    }
    expected_types = [
      {
        'type': 'object',
        'id': 'FooCommandParams',
        'description': 'Parameters for the FooCommand command.',
        'properties': [],
      },
      {
        'type': 'object',
        'id': 'FooCommandResult',
        'description': 'Result for the FooCommand command.',
        'properties': [],
      },
      {
        'type': 'object',
        'id': 'BarEventParams',
        'description': 'Parameters for the BarEvent event.',
        'properties': [],
      }
    ]
    client_api_generator.EnsureCommandsHaveParametersAndReturnTypes(json_api)
    client_api_generator.SynthesizeCommandTypes(json_api)
    client_api_generator.SynthesizeEventTypes(json_api)
    types = json_api['domains'][0]['types']
    self.assertListEqual(types, expected_types)

  def test_Generate(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain',
          'types': [
            {
              'id': 'TestType',
              'type': 'object',
              'properties': [
                  {'name': 'p1', 'type': 'number'},
                  {'name': 'p2', 'type': 'integer'},
                  {'name': 'p3', 'type': 'boolean'},
                  {'name': 'p4', 'type': 'string'},
                  {'name': 'p5', 'type': 'any'},
                  {'name': 'p6', 'type': 'object', '$ref': 'domain.TestType'},
              ],
              'returns': [
                  {'name': 'r1', 'type': 'number'},
                  {'name': 'r2', 'type': 'integer'},
                  {'name': 'r3', 'type': 'boolean'},
                  {'name': 'r4', 'type': 'string'},
                  {'name': 'r5', 'type': 'any'},
                  {'name': 'r6', 'type': 'object', '$ref': 'domain.TestType'},
              ],
            },
          ]
        },
      ]
    }
    try:
      dirname = tempfile.mkdtemp()
      jinja_env = client_api_generator.InitializeJinjaEnv(dirname)
      client_api_generator.CreateTypeDefinitions(json_api)
      client_api_generator.GenerateDomains(jinja_env, dirname, json_api)
      # This is just a smoke test; we don't actually verify the generated output
      # here.
    finally:
      shutil.rmtree(dirname)

  def test_GenerateDomains(self):
    json_api = {
      'domains': [
        {
          'domain': 'domain0',
          'types': [
            {
              'id': 'TestType',
              'type': 'object',
            },
          ]
        },
        {
          'domain': 'domain1',
          'types': [
            {
              'id': 'TestType',
              'type': 'object',
            },
          ]
        },
      ]
    }
    try:
      dirname = tempfile.mkdtemp()
      jinja_env = client_api_generator.InitializeJinjaEnv(dirname)
      client_api_generator.GeneratePerDomain(
          jinja_env, dirname, json_api,
          'domain', ['cc', 'h'], lambda domain_name: domain_name)
      # This is just a smoke test; we don't actually verify the generated output
      # here.
    finally:
      shutil.rmtree(dirname)


if __name__ == '__main__':
  unittest.main(verbosity=2, exit=False, argv=sys.argv)
