#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for setup-session-environment."""

import importlib.machinery
import importlib.util
import json
import os
import subprocess
import sys
import unittest
from unittest import mock

try:
  import dbus
  if not hasattr(dbus, 'SessionBus'):
    raise ImportError('dbus module lacks SessionBus')
except ImportError:
  dbus = mock.MagicMock()
  sys.modules['dbus'] = dbus

# Load setup-session-environment dynamically since it has hyphens in the name
# and lacks a .py extension.
SCRIPT_PATH = os.path.join(
    os.path.dirname(os.path.realpath(__file__)), 'setup-session-environment'
)
loader = importlib.machinery.SourceFileLoader(
    'setup_session_environment', SCRIPT_PATH
)
spec = importlib.util.spec_from_loader('setup_session_environment', loader)
setup_session_environment = importlib.util.module_from_spec(spec)
sys.modules['setup_session_environment'] = setup_session_environment
spec.loader.exec_module(setup_session_environment)


class SystemdManagerTest(unittest.TestCase):

  def _make_mock_manager(self):
    mock_bus = mock.MagicMock()
    mock_obj = mock.MagicMock()
    mock_props = mock.MagicMock()
    mock_iface = mock.MagicMock()
    mock_bus.get_object.return_value = mock_obj

    def interface_side_effect(obj, iface_name):
      if iface_name == 'org.freedesktop.DBus.Properties':
        return mock_props
      elif iface_name == 'org.freedesktop.systemd1.Manager':
        return mock_iface
      return mock.MagicMock()

    return mock_bus, mock_props, mock_iface, interface_side_effect

  @mock.patch('dbus.Interface')
  @mock.patch('dbus.SessionBus')
  def test_get_environment_dbus_success(self, mock_session_bus, mock_interface):
    mock_bus, mock_props, _, iface_side_effect = self._make_mock_manager()
    mock_session_bus.return_value = mock_bus
    mock_interface.side_effect = iface_side_effect
    mock_props.Get.return_value = [
        'FOO=bar',
        'SSH_AUTH_SOCK=/run/user/1000/space path/agent.sock',
        'EMPTY=',
        'NO_EQUALS',
    ]
    mgr = setup_session_environment.SystemdManager()
    env = mgr.get_environment()
    self.assertEqual(
        env,
        {
            'FOO': 'bar',
            'SSH_AUTH_SOCK': '/run/user/1000/space path/agent.sock',
            'EMPTY': '',
        },
    )
    for k, v in env.items():
      self.assertIs(type(k), str)
      self.assertIs(type(v), str)

    mock_bus.get_object.assert_called_once_with(
        'org.freedesktop.systemd1', '/org/freedesktop/systemd1'
    )
    mock_props.Get.assert_called_once_with(
        'org.freedesktop.systemd1.Manager', 'Environment'
    )

  @mock.patch('dbus.SessionBus', side_effect=Exception('D-Bus connect error'))
  def test_init_dbus_error(self, mock_session_bus):
    mgr = setup_session_environment.SystemdManager()
    self.assertIsNone(mgr.bus)
    self.assertEqual(mgr.get_environment(), {})
    mgr.set_environment(['A=1'])
    mgr.unset_environment(['A'])

  @mock.patch('os.getuid', return_value=1234)
  @mock.patch('dbus.SessionBus')
  def test_init_sets_dbus_session_bus_address_fallback(
      self, mock_session_bus, mock_getuid
  ):
    with mock.patch.dict(os.environ, {}, clear=True):
      setup_session_environment.SystemdManager()
      self.assertEqual(
          os.environ.get('DBUS_SESSION_BUS_ADDRESS'),
          'unix:path=/run/user/1234/bus',
      )

  @mock.patch('os.getuid', return_value=1234)
  @mock.patch('dbus.SessionBus')
  def test_init_preserves_existing_dbus_session_bus_address(
      self, mock_session_bus, mock_getuid
  ):
    custom_addr = 'unix:abstract=/tmp/dbus-custom'
    with mock.patch.dict(
        os.environ, {'DBUS_SESSION_BUS_ADDRESS': custom_addr}, clear=True
    ):
      setup_session_environment.SystemdManager()
      self.assertEqual(os.environ.get('DBUS_SESSION_BUS_ADDRESS'), custom_addr)

  @mock.patch('dbus.Interface')
  @mock.patch('dbus.SessionBus')
  def test_set_environment(self, mock_session_bus, mock_interface):
    mock_bus, _, mock_iface, iface_side_effect = self._make_mock_manager()
    mock_session_bus.return_value = mock_bus
    mock_interface.side_effect = iface_side_effect
    mgr = setup_session_environment.SystemdManager()

    mgr.set_environment([])
    mock_iface.SetEnvironment.assert_not_called()

    mgr.set_environment(['A=1', 'B=2'])
    mock_iface.SetEnvironment.assert_called_once_with(['A=1', 'B=2'])

  @mock.patch('dbus.Interface')
  @mock.patch('dbus.SessionBus')
  def test_unset_environment(self, mock_session_bus, mock_interface):
    mock_bus, _, mock_iface, iface_side_effect = self._make_mock_manager()
    mock_session_bus.return_value = mock_bus
    mock_interface.side_effect = iface_side_effect
    mgr = setup_session_environment.SystemdManager()

    mgr.unset_environment([])
    mock_iface.UnsetEnvironment.assert_not_called()

    mgr.unset_environment(['A', 'B'])
    mock_iface.UnsetEnvironment.assert_called_once_with(['A', 'B'])

  @mock.patch('dbus.Interface')
  @mock.patch('dbus.SessionBus')
  def test_dbus_call_error_handling(self, mock_session_bus, mock_interface):
    mock_bus, mock_props, mock_iface, iface_side_effect = (
        self._make_mock_manager()
    )
    mock_session_bus.return_value = mock_bus
    mock_interface.side_effect = iface_side_effect
    mock_props.Get.side_effect = Exception('Get failed')
    mock_iface.SetEnvironment.side_effect = Exception('Set failed')
    mock_iface.UnsetEnvironment.side_effect = Exception('Unset failed')

    mgr = setup_session_environment.SystemdManager()
    self.assertEqual(mgr.get_environment(), {})
    mgr.set_environment(['A=1'])
    mgr.unset_environment(['A'])


class EnvironmentStoreTest(unittest.TestCase):

  def test_load_missing_file(self):
    store = setup_session_environment.EnvironmentStore(
        path='/nonexistent/state.json'
    )
    self.assertFalse(store.load())
    self.assertFalse(store.is_valid)
    self.assertEqual(store.variables, {})

  @mock.patch('os.path.exists', return_value=True)
  def test_load_corrupted_or_invalid_json(self, mock_exists):
    store = setup_session_environment.EnvironmentStore(path='/tmp/state.json')
    bad_payloads = ('not valid json', '[]', '{"environment": "not-dict"}')
    for payload in bad_payloads:
      with self.subTest(payload=payload):
        with mock.patch('builtins.open', mock.mock_open(read_data=payload)):
          self.assertFalse(store.load())
          self.assertFalse(store.is_valid)

  @mock.patch('os.path.exists', return_value=True)
  def test_load_valid_and_backwards_compatible(self, mock_exists):
    store = setup_session_environment.EnvironmentStore(path='/tmp/state.json')
    content = json.dumps({
        'environment': {
            'NEW_STYLE': {
                'original_value': 'orig1',
                'injected_value': 'inj1',
            },
            'OLD_STYLE': {
                'previous': 'orig2',
                'overridden': 'inj2',
            },
        }
    })
    with mock.patch('builtins.open', mock.mock_open(read_data=content)):
      self.assertTrue(store.load())
      self.assertTrue(store.is_valid)
      self.assertEqual(store.get_entry('NEW_STYLE'), ('orig1', 'inj1'))
      self.assertEqual(store.get_entry('OLD_STYLE'), ('orig2', 'inj2'))
      self.assertEqual(store.get_entry('NONEXISTENT'), (None, None))

  def test_record_and_get_entry(self):
    store = setup_session_environment.EnvironmentStore(path='/tmp/state.json')
    store.record('VAR', 'original', 'injected')
    self.assertEqual(store.get_entry('VAR'), ('original', 'injected'))

  @mock.patch('os.replace')
  @mock.patch('os.open')
  def test_save_atomic_and_permissions(self, mock_os_open, mock_replace):
    mock_os_open.return_value = 3
    store = setup_session_environment.EnvironmentStore(path='/tmp/state.json')
    store.record('SSH_AUTH_SOCK', '/old.sock', '/injected.sock')

    mock_file_open = mock.mock_open()
    with mock.patch('builtins.open', mock_file_open):
      store.save()

    mock_os_open.assert_called_once_with(
        mock.ANY,
        os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_TRUNC,
        0o600,
    )
    mock_replace.assert_called_once()
    handle = mock_file_open()
    written = ''.join(c.args[0] for c in handle.write.call_args_list)
    self.assertEqual(
        json.loads(written),
        {
            'environment': {
                'SSH_AUTH_SOCK': {
                    'original_value': '/old.sock',
                    'injected_value': '/injected.sock',
                }
            }
        },
    )

  @mock.patch('os.path.exists', return_value=True)
  @mock.patch('os.remove')
  def test_cleanup(self, mock_remove, mock_exists):
    store = setup_session_environment.EnvironmentStore(path='/tmp/state.json')
    store.cleanup()
    mock_remove.assert_called_once_with('/tmp/state.json')


class SetupSessionEnvironmentTest(unittest.TestCase):

  def setUp(self):
    self.runtime_dir = '/run/user/1000'
    self.state_file_path = os.path.join(
        self.runtime_dir, 'crd_previous_session_state.json'
    )

  def test_get_saved_session_state_path(self):
    with mock.patch.dict(os.environ, {}, clear=True):
      with mock.patch('os.getuid', return_value=1234):
        self.assertEqual(
            setup_session_environment._get_saved_session_state_path(),
            '/run/user/1234/crd_previous_session_state.json',
        )
    with mock.patch.dict(os.environ, {'XDG_RUNTIME_DIR': '/tmp/runtime'}):
      self.assertEqual(
          setup_session_environment._get_saved_session_state_path(),
          '/tmp/runtime/crd_previous_session_state.json',
      )

  @mock.patch('os.path.exists')
  @mock.patch('subprocess.check_output')
  @mock.patch.object(
      setup_session_environment.SystemdManager, 'set_environment'
  )
  def test_start_non_crd_or_error_does_not_set_env(
      self, mock_set_env, mock_check_output, mock_exists
  ):
    test_cases = (
        ('missing_binary', False, None, None),
        ('binary_error', True, subprocess.CalledProcessError(1, 'cmd'), None),
        ('not_crd', True, None, json.dumps({'isCrdSession': False})),
        ('non_dict', True, None, json.dumps(['not', 'a', 'dict'])),
    )
    for name, exists_val, error, output in test_cases:
      with self.subTest(name=name):
        mock_set_env.reset_mock()
        mock_exists.return_value = exists_val
        mock_check_output.side_effect = error
        mock_check_output.return_value = output
        setup_session_environment.start()
        mock_set_env.assert_not_called()

  @mock.patch('os.open')
  @mock.patch('os.replace')
  @mock.patch('os.path.exists')
  @mock.patch.object(
      setup_session_environment.SystemdManager, 'get_environment'
  )
  @mock.patch.object(
      setup_session_environment.SystemdManager, 'set_environment'
  )
  @mock.patch('subprocess.check_output')
  def test_start_crd_session_state_creation(
      self,
      mock_check_output,
      mock_set_env,
      mock_get_env,
      mock_exists,
      mock_replace,
      mock_os_open,
  ):
    mock_exists.side_effect = lambda p: not p.endswith(
        'crd_previous_session_state.json'
    )
    mock_check_output.return_value = json.dumps({
        'isCrdSession': True,
        'sshAuthSock': '/run/user/1000/space path/crd_sock',
    })

    cases = (
        (
            'with_prev',
            {'SSH_AUTH_SOCK': '/run/user/1000/space path/agent.sock'},
            '/run/user/1000/space path/agent.sock',
        ),
        ('no_prev', {}, None),
    )
    for name, env_val, expected_prev in cases:
      with self.subTest(name=name):
        mock_set_env.reset_mock()
        mock_get_env.return_value = env_val
        mock_file_open = mock.mock_open()
        with mock.patch('builtins.open', mock_file_open):
          setup_session_environment.start()

        handle = mock_file_open()
        written = ''.join(c.args[0] for c in handle.write.call_args_list)
        self.assertEqual(
            json.loads(written),
            {
                'environment': {
                    'SSH_AUTH_SOCK': {
                        'original_value': expected_prev,
                        'injected_value': (
                            '/run/user/1000/space path/crd_sock'
                        ),
                    }
                }
            },
        )
        mock_set_env.assert_called_once_with([
            'CHROME_REMOTE_DESKTOP_SESSION=1',
            'SSH_AUTH_SOCK=/run/user/1000/space path/crd_sock',
        ])

  @mock.patch('os.open')
  @mock.patch('os.replace')
  @mock.patch('os.path.exists', return_value=True)
  @mock.patch.object(
      setup_session_environment.SystemdManager, 'get_environment'
  )
  @mock.patch.object(
      setup_session_environment.SystemdManager, 'set_environment'
  )
  @mock.patch('subprocess.check_output')
  def test_start_reconnection_preserves_original_and_updates_injected(
      self,
      mock_check_output,
      mock_set_env,
      mock_get_env,
      mock_exists,
      mock_replace,
      mock_os_open,
  ):
    existing_state = json.dumps({
        'environment': {
            'SSH_AUTH_SOCK': {
                'original_value': '/run/user/1000/original_agent',
                'injected_value': '/run/user/1000/crd_sock_1',
            }
        }
    })
    mock_check_output.return_value = json.dumps({
        'isCrdSession': True,
        'sshAuthSock': '/run/user/1000/crd_sock_2',
    })
    mock_get_env.return_value = {'SSH_AUTH_SOCK': '/run/user/1000/crd_sock_2'}

    mock_file_open = mock.mock_open(read_data=existing_state)
    with mock.patch('builtins.open', mock_file_open):
      setup_session_environment.start()

    handle = mock_file_open()
    written = ''.join(c.args[0] for c in handle.write.call_args_list)
    self.assertEqual(
        json.loads(written),
        {
            'environment': {
                'SSH_AUTH_SOCK': {
                    'original_value': '/run/user/1000/original_agent',
                    'injected_value': '/run/user/1000/crd_sock_2',
                }
            }
        },
    )
    mock_set_env.assert_called_once_with([
        'CHROME_REMOTE_DESKTOP_SESSION=1',
        'SSH_AUTH_SOCK=/run/user/1000/crd_sock_2',
    ])

  @mock.patch('os.open')
  @mock.patch('os.replace')
  @mock.patch('os.path.exists', return_value=True)
  @mock.patch.object(
      setup_session_environment.SystemdManager, 'get_environment'
  )
  @mock.patch.object(
      setup_session_environment.SystemdManager, 'set_environment'
  )
  @mock.patch('subprocess.check_output')
  def test_start_reconnection_newly_enables_ssh_forwarding(
      self,
      mock_check_output,
      mock_set_env,
      mock_get_env,
      mock_exists,
      mock_replace,
      mock_os_open,
  ):
    empty_state = json.dumps({'environment': {}})
    mock_check_output.return_value = json.dumps({
        'isCrdSession': True,
        'sshAuthSock': '/run/user/1000/crd_sock_new',
    })
    mock_get_env.return_value = {'SSH_AUTH_SOCK': '/run/user/1000/local_agent'}

    mock_file_open = mock.mock_open(read_data=empty_state)
    with mock.patch('builtins.open', mock_file_open):
      setup_session_environment.start()

    handle = mock_file_open()
    written = ''.join(c.args[0] for c in handle.write.call_args_list)
    self.assertEqual(
        json.loads(written),
        {
            'environment': {
                'SSH_AUTH_SOCK': {
                    'original_value': '/run/user/1000/local_agent',
                    'injected_value': '/run/user/1000/crd_sock_new',
                }
            }
        },
    )
    mock_set_env.assert_called_once_with([
        'CHROME_REMOTE_DESKTOP_SESSION=1',
        'SSH_AUTH_SOCK=/run/user/1000/crd_sock_new',
    ])

  def _run_stop_test(
      self,
      state_content,
      env,
      expected_unsets,
      expected_sets,
      file_exists=True,
  ):
    with mock.patch('os.remove') as mock_remove:
      with mock.patch.object(
          setup_session_environment.SystemdManager,
          'get_environment',
          return_value=env,
      ):
        with mock.patch.object(
            setup_session_environment.SystemdManager, 'unset_environment'
        ) as mock_unset_env:
          with mock.patch.object(
              setup_session_environment.SystemdManager, 'set_environment'
          ) as mock_set_env:
            with mock.patch('os.path.exists', return_value=file_exists):
              with mock.patch(
                  'builtins.open', mock.mock_open(read_data=state_content)
              ):
                setup_session_environment.stop()

            if file_exists:
              mock_remove.assert_called_once()
            else:
              mock_remove.assert_not_called()

            mock_unset_env.assert_called_once_with(expected_unsets)
            mock_set_env.assert_called_once_with(expected_sets)

  def test_stop_normal_restorations(self):
    state_orig = json.dumps({
        'environment': {
            'SSH_AUTH_SOCK': {
                'original_value': (
                    '/run/user/1000/space path/openssh_agent.sock'
                ),
                'injected_value': '/run/user/1000/space path/crd_sock',
            }
        }
    })
    self._run_stop_test(
        state_content=state_orig,
        env={
            'CHROME_REMOTE_DESKTOP_SESSION': '1',
            'SSH_AUTH_SOCK': '/run/user/1000/space path/crd_sock',
        },
        expected_unsets=['CHROME_REMOTE_DESKTOP_SESSION'],
        expected_sets=[
            'SSH_AUTH_SOCK=/run/user/1000/space path/openssh_agent.sock'
        ],
    )

    state_no_orig = json.dumps({
        'environment': {
            'SSH_AUTH_SOCK': {
                'original_value': None,
                'injected_value': '/run/user/1000/crd_sock',
            }
        }
    })
    self._run_stop_test(
        state_content=state_no_orig,
        env={
            'CHROME_REMOTE_DESKTOP_SESSION': '1',
            'SSH_AUTH_SOCK': '/run/user/1000/crd_sock',
        },
        expected_unsets=['CHROME_REMOTE_DESKTOP_SESSION', 'SSH_AUTH_SOCK'],
        expected_sets=[],
    )

  def test_stop_legacy_state_file_format(self):
    state_legacy = json.dumps({
        'environment': {
            'SSH_AUTH_SOCK': {
                'previous': '/run/user/1000/openssh_agent',
                'overridden': '/run/user/1000/crd_sock',
            }
        }
    })
    self._run_stop_test(
        state_content=state_legacy,
        env={
            'CHROME_REMOTE_DESKTOP_SESSION': '1',
            'SSH_AUTH_SOCK': '/run/user/1000/crd_sock',
        },
        expected_unsets=['CHROME_REMOTE_DESKTOP_SESSION'],
        expected_sets=['SSH_AUTH_SOCK=/run/user/1000/openssh_agent'],
    )

  def test_stop_user_modifications_preserved(self):
    state = json.dumps({
        'environment': {
            'SSH_AUTH_SOCK': {
                'original_value': '/run/user/1000/openssh_agent',
                'injected_value': '/run/user/1000/crd_sock',
            }
        }
    })
    self._run_stop_test(
        state_content=state,
        env={
            'CHROME_REMOTE_DESKTOP_SESSION': '1',
            'SSH_AUTH_SOCK': '/custom/user/modified_agent.sock',
        },
        expected_unsets=['CHROME_REMOTE_DESKTOP_SESSION'],
        expected_sets=[],
    )

    state_none_injected = json.dumps({
        'environment': {
            'SSH_AUTH_SOCK': {
                'original_value': None,
                'injected_value': None,
            }
        }
    })
    self._run_stop_test(
        state_content=state_none_injected,
        env={
            'CHROME_REMOTE_DESKTOP_SESSION': '1',
            'SSH_AUTH_SOCK': '/custom/user/manual.sock',
        },
        expected_unsets=['CHROME_REMOTE_DESKTOP_SESSION'],
        expected_sets=[],
    )

  def test_stop_no_ssh_forwarding_preserves_local_socket(self):
    self._run_stop_test(
        state_content=json.dumps({'environment': {}}),
        env={
            'CHROME_REMOTE_DESKTOP_SESSION': '1',
            'SSH_AUTH_SOCK': '/run/user/1000/local_agent',
        },
        expected_unsets=['CHROME_REMOTE_DESKTOP_SESSION'],
        expected_sets=[],
    )

  def test_stop_unrelated_environment_variables_in_state_ignored(self):
    state = json.dumps({
        'environment': {
            'SSH_AUTH_SOCK': {
                'original_value': '/run/user/1000/openssh_agent',
                'injected_value': '/run/user/1000/crd_sock',
            },
            'UNRELATED': {'original_value': 'old', 'injected_value': 'new'},
        }
    })
    self._run_stop_test(
        state_content=state,
        env={
            'CHROME_REMOTE_DESKTOP_SESSION': '1',
            'SSH_AUTH_SOCK': '/run/user/1000/crd_sock',
            'UNRELATED': 'new',
        },
        expected_unsets=['CHROME_REMOTE_DESKTOP_SESSION'],
        expected_sets=['SSH_AUTH_SOCK=/run/user/1000/openssh_agent'],
    )

  def test_stop_corrupted_state_or_missing_file_fallback(self):
    cases = (
        (
            'invalid_json',
            'invalid json!',
            True,
            ['CHROME_REMOTE_DESKTOP_SESSION', 'SSH_AUTH_SOCK'],
        ),
        (
            'missing_file_in_crd',
            '',
            False,
            ['CHROME_REMOTE_DESKTOP_SESSION', 'SSH_AUTH_SOCK'],
        ),
        ('missing_file_not_crd', '', False, ['CHROME_REMOTE_DESKTOP_SESSION']),
    )
    for name, content, file_exists, expected_unsets in cases:
      with self.subTest(name=name):
        env = {'SSH_AUTH_SOCK': '/run/user/1000/sock'}
        if 'not_crd' not in name:
          env['CHROME_REMOTE_DESKTOP_SESSION'] = '1'
        self._run_stop_test(
            state_content=content,
            env=env,
            expected_unsets=expected_unsets,
            expected_sets=[],
            file_exists=file_exists,
        )

  @mock.patch('os.path.exists', return_value=True)
  @mock.patch('os.remove')
  @mock.patch.object(
      setup_session_environment.SystemdManager, 'get_environment'
  )
  def test_stop_unset_failure_does_not_prevent_restore_set(
      self, mock_get_env, mock_remove, mock_exists
  ):
    mock_get_env.return_value = {
        'CHROME_REMOTE_DESKTOP_SESSION': '1',
        'SSH_AUTH_SOCK': '/run/user/1000/crd_sock',
    }
    state_content = json.dumps({
        'environment': {
            'SSH_AUTH_SOCK': {
                'original_value': '/run/user/1000/openssh_agent',
                'injected_value': '/run/user/1000/crd_sock',
            }
        }
    })

    systemd_mgr = setup_session_environment.SystemdManager()
    mock_interface = mock.MagicMock()
    mock_interface.UnsetEnvironment.side_effect = Exception('Unset failed')
    systemd_mgr.interface = mock_interface

    with mock.patch('builtins.open', mock.mock_open(read_data=state_content)):
      with mock.patch(
          'setup_session_environment.SystemdManager', return_value=systemd_mgr
      ):
        setup_session_environment.stop()

    mock_remove.assert_called_once()
    mock_interface.UnsetEnvironment.assert_called_once_with(
        ['CHROME_REMOTE_DESKTOP_SESSION']
    )
    mock_interface.SetEnvironment.assert_called_once_with(
        ['SSH_AUTH_SOCK=/run/user/1000/openssh_agent']
    )


if __name__ == '__main__':
  unittest.main()
