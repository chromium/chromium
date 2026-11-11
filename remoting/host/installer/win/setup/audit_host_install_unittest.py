#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for audit_host_install.py."""

import io
import json
from typing import Any, Optional
import unittest
from unittest import mock

import audit_host_install

PROGRAM_FILES_SDDL = (
  f'O:{audit_host_install.TRUSTED_INSTALLER_SID}G:BAD:PAI'
  '(A;OICIIO;GA;;;CO)'
  '(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)'
  '(A;OICI;GRGX;;;BU)'
  '(A;OICIIO;GXGR;;;AC)(A;;0x1200a9;;;AC)'
)
LEAF_PAYLOAD_SDDL = (
  f'O:{audit_host_install.TRUSTED_INSTALLER_SID}G:BAD:AI'
  '(A;ID;FA;;;SY)(A;ID;FA;;;BA)'
  '(A;ID;0x1200a9;;;BU)(A;ID;0x1200a9;;;AC)'
)
HKLM_DEFAULT_SDDL = (
  'O:BAG:BAD:(A;CI;KA;;;SY)(A;CI;KA;;;BA)(A;CI;KR;;;BU)(A;CIIO;KA;;;CO)'
)
SECRETS_REG_SDDL = 'O:BAG:BAD:P(A;;GA;;;BA)(A;;GA;;;SY)'
CONFIG_DIR_SDDL = (
  'O:BAG:BAD:P(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)(A;OICI;GRGX;;;BU)'
)
HOST_JSON_SDDL = 'O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)'
UNPRIV_JSON_SDDL = 'O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;AU)'
SERVICE_DEFAULT_SDDL = (
  'O:SYG:SYD:(A;;CCLCSWRPWPDTLOCRRC;;;SY)'
  '(A;;CCDCLCSWRPWPDTLOCRSDRCWDWO;;;BA)'
  '(A;;CCLCSWLOCRRC;;;IU)(A;;CCLCSWLOCRRC;;;SU)'
)
VALID_COM_SDDL = (
  'O:BAG:BAD:(A;;CCDCSW;;;SY)(A;;CCDCSW;;;BA)(A;;CCDCSW;;;LS)S:(ML;;NX;;;ME)'
)
DEFAULT_NATIVE_VERSION = '146.0.7650.0'


class FakeSystemProbe(audit_host_install.SystemProbe):
  """In-memory fake implementation of SystemProbe for unit testing."""

  def __init__(self) -> None:
    self.env: dict[str, str] = {
      'ProgramW6432': r'C:\Program Files',
      'ProgramFiles': r'C:\Program Files',
      'ProgramFiles(x86)': r'C:\Program Files (x86)',
      'ProgramData': r'C:\ProgramData',
    }
    self.files: set[str] = set()
    self.dirs: dict[str, set[str]] = {}
    self.reparse_points: set[str] = set()
    self.file_sddls: dict[str, str] = {}
    self.reg: dict[tuple[int, str], dict[str, Any]] = {}
    self.reg_case: dict[tuple[int, str], str] = {}
    self.reg_sddls: dict[tuple[int, str], str] = {}
    self.default_reg_sddl: Optional[str] = HKLM_DEFAULT_SDDL
    self.binary_sds: dict[bytes, str] = {}
    self.service = audit_host_install.ServiceInfo(exists=False)
    self.service_sddl: Optional[str] = SERVICE_DEFAULT_SDDL
    self.msi_products: list[str] = []

  def _norm(self, path: str) -> str:
    return path.rstrip('\\/').lower()

  def add_dir(
    self, path: str, sddl: Optional[str] = None, reparse: bool = False
  ) -> None:
    norm = self._norm(path)
    self.dirs.setdefault(norm, set())
    if reparse:
      self.reparse_points.add(norm)
    if sddl is not None:
      self.file_sddls[norm] = sddl
    if '\\' in norm:
      parent, name = path.rstrip('\\/').rsplit('\\', 1)
      self.add_dir(parent)
      self.dirs[self._norm(parent)].add(name)

  def add_file(
    self, path: str, sddl: Optional[str] = None, reparse: bool = False
  ) -> None:
    norm = self._norm(path)
    self.files.add(norm)
    if reparse:
      self.reparse_points.add(norm)
    if sddl is not None:
      self.file_sddls[norm] = sddl
    parent, name = path.rstrip('\\/').rsplit('\\', 1)
    self.add_dir(parent)
    self.dirs[self._norm(parent)].add(name)

  def remove_file(self, path: str) -> None:
    norm = self._norm(path)
    self.files.discard(norm)
    if '\\' in norm:
      parent, name = path.rstrip('\\/').rsplit('\\', 1)
      if self._norm(parent) in self.dirs:
        self.dirs[self._norm(parent)].discard(name)

  def set_file_sddl(self, path: str, sddl: str) -> None:
    self.file_sddls[self._norm(path)] = sddl

  def remove_file_sddl(self, path: str) -> None:
    self.file_sddls.pop(self._norm(path), None)

  def add_reparse_point(self, path: str) -> None:
    self.reparse_points.add(self._norm(path))

  def set_reg_key(
    self, subkey: str, view: int = 64, sddl: Optional[str] = None
  ) -> None:
    parts = subkey.strip('\\').split('\\')
    accum: list[str] = []
    for part in parts:
      accum.append(part)
      cur = '\\'.join(accum)
      key = (view, cur.lower())
      self.reg.setdefault(key, {})
      self.reg_case[key] = cur
    if sddl is not None:
      self.reg_sddls[(view, subkey.strip('\\').lower())] = sddl

  def set_reg_value(
    self, subkey: str, value_name: str, value: Any, view: int = 64
  ) -> None:
    self.set_reg_key(subkey, view=view)
    self.reg[(view, subkey.strip('\\').lower())][value_name] = value

  def remove_reg_key(self, subkey: str, view: int = 64) -> None:
    key = (view, subkey.strip('\\').lower())
    self.reg.pop(key, None)
    self.reg_case.pop(key, None)
    self.reg_sddls.pop(key, None)

  def remove_reg_value(
    self, subkey: str, value_name: str, view: int = 64
  ) -> None:
    vals = self.reg.get((view, subkey.strip('\\').lower()))
    if vals is not None:
      vals.pop(value_name, None)

  def register_binary_sd(self, raw_bytes: bytes, sddl: str) -> bytes:
    self.binary_sds[raw_bytes] = sddl
    return raw_bytes

  def get_env(self, name: str) -> Optional[str]:
    return self.env.get(name)

  def path_exists(self, path: str) -> bool:
    n = self._norm(path)
    return n in self.files or n in self.dirs or n in self.reparse_points

  def is_dir(self, path: str) -> bool:
    return self._norm(path) in self.dirs

  def is_reparse_point(self, path: str) -> bool:
    return self._norm(path) in self.reparse_points

  def list_dir(self, path: str) -> list[str]:
    return sorted(self.dirs.get(self._norm(path), set()))

  def get_file_sddl(self, path: str) -> Optional[str]:
    return self.file_sddls.get(self._norm(path))

  def reg_key_exists(self, subkey: str, view: int = 64) -> bool:
    return (view, subkey.strip('\\').lower()) in self.reg

  def reg_get_value(
    self, subkey: str, value_name: str, view: int = 64
  ) -> Optional[Any]:
    vals = self.reg.get((view, subkey.strip('\\').lower()))
    return None if vals is None else vals.get(value_name)

  def reg_list_subkeys(self, subkey: str, view: int = 64) -> list[str]:
    prefix = subkey.strip('\\').lower() + '\\'
    children: set[str] = set()
    for (v, k), orig in self.reg_case.items():
      if v == view and k.startswith(prefix):
        rem = orig[len(prefix) :]
        if rem and '\\' not in rem:
          children.add(rem)
    return sorted(children)

  def reg_list_values(self, subkey: str, view: int = 64) -> list[str]:
    vals = self.reg.get((view, subkey.strip('\\').lower()), {})
    return sorted(vals.keys())

  def get_reg_sddl(self, subkey: str, view: int = 64) -> Optional[str]:
    key = (view, subkey.strip('\\').lower())
    if key not in self.reg:
      return None
    return self.reg_sddls.get(key, self.default_reg_sddl)

  def binary_sd_to_sddl(self, raw_sd: Any) -> Optional[str]:
    if not isinstance(raw_sd, bytes) or len(raw_sd) < 20:
      return None
    return self.binary_sds.get(raw_sd)

  def query_service(self, service_name: str) -> audit_host_install.ServiceInfo:
    return self.service

  def get_service_sddl(self, service_name: str) -> Optional[str]:
    return self.service_sddl if self.service.exists else None

  def enum_msi_products(self, upgrade_code: str) -> list[str]:
    return list(self.msi_products)


def _populate_complete_native_install(
  probe: FakeSystemProbe, version: str = DEFAULT_NATIVE_VERSION
) -> None:
  install_dir = r'C:\Program Files\Google\Chrome Remote Desktop'
  config_dir = r'C:\ProgramData\Google\Chrome Remote Desktop'
  probe.add_dir(install_dir, sddl=PROGRAM_FILES_SDDL)
  for fname in audit_host_install.EXPECTED_PAYLOAD_FILES:
    probe.add_file(rf'{install_dir}\{fname}', sddl=LEAF_PAYLOAD_SDDL)

  probe.add_dir(config_dir, sddl=CONFIG_DIR_SDDL)
  probe.add_file(rf'{config_dir}\host.json', sddl=HOST_JSON_SDDL)
  probe.add_file(rf'{config_dir}\host_unprivileged.json', sddl=UNPRIV_JSON_SDDL)

  svc_key = (
    rf'SYSTEM\CurrentControlSet\Services\{audit_host_install.SERVICE_NAME}'
  )
  probe.set_reg_key(svc_key, view=64)
  probe.service = audit_host_install.ServiceInfo(
    exists=True,
    state='RUNNING',
    start_type='AUTO_START',
    account='LocalSystem',
    binary_path=(
      rf'"{install_dir}\remoting_host.exe" --type=daemon '
      rf'--host-config="{config_dir}\host.json"'
    ),
  )

  omaha = rf'SOFTWARE\Google\Update\Clients\{audit_host_install.OMAHA_APP_ID}'
  for view in (64, 32):
    probe.set_reg_value(omaha, 'pv', version, view=view)
    probe.set_reg_value(omaha, 'name', 'Chrome Remote Desktop Host', view=view)

  manifest_map = {
    'com.google.chrome.remote_assistance': (
      'com.google.chrome.remote_assistance.json',
      'com.google.chrome.remote_assistance-firefox.json',
    ),
    'com.google.chrome.remote_desktop': (
      'com.google.chrome.remote_desktop.json',
      'com.google.chrome.remote_desktop-firefox.json',
    ),
    'com.google.chrome.remote_webauthn': (
      'com.google.chrome.remote_webauthn.json',
      '',
    ),
  }
  for subkey in audit_host_install.NATIVE_MESSAGING_HOSTS:
    host_name = subkey.rsplit('\\', 1)[1]
    is_moz = 'Mozilla' in subkey
    manifest = manifest_map[host_name][1 if is_moz else 0]
    for view in (64, 32):
      probe.set_reg_value(subkey, '', rf'{install_dir}\{manifest}', view=view)

  ev_key = r'SYSTEM\CurrentControlSet\Services\EventLog\Application\chromoting'
  probe.set_reg_value(
    ev_key, 'EventMessageFile', rf'{install_dir}\remoting_core.dll', view=64
  )
  probe.set_reg_value(
    ev_key, 'CategoryMessageFile', rf'{install_dir}\remoting_core.dll', view=64
  )
  probe.set_reg_value(
    r'SOFTWARE\Google\SecurityKey',
    'RemoteSecurityKeyPath',
    rf'{install_dir}\remote_security_key.exe',
    view=64,
  )

  com_sd = probe.register_binary_sd(
    b'\x01\x00\x04\x80' + b'\x00' * 16 + b'valid', VALID_COM_SDDL
  )
  app_id = rf'SOFTWARE\Classes\AppId\{audit_host_install.RDP_APP_ID}'
  probe.set_reg_value(app_id, 'RunAs', audit_host_install.RDP_RUN_AS, view=64)
  probe.set_reg_value(app_id, 'AccessPermission', com_sd, view=64)
  probe.set_reg_value(app_id, 'LaunchPermission', com_sd, view=64)

  tlb_id = audit_host_install.CHROMOTING_TYPELIB_ID
  rdp_clsid = audit_host_install.compute_rdp_desktop_session_clsid(version)
  probe.set_reg_value(
    rf'SOFTWARE\Classes\CLSID\{rdp_clsid}',
    'AppID',
    audit_host_install.RDP_APP_ID,
    view=64,
  )
  probe.set_reg_value(
    rf'SOFTWARE\Classes\CLSID\{rdp_clsid}\LocalServer32',
    '',
    rf'"{install_dir}\remoting_curtain.exe" --type=rdp_desktop_session',
    view=64,
  )
  probe.set_reg_value(
    rf'SOFTWARE\Classes\CLSID\{rdp_clsid}\TypeLib', '', tlb_id, view=64
  )

  for ps in (
    audit_host_install.IRDP_DESKTOP_SESSION_ID,
    audit_host_install.IRDP_DESKTOP_SESSION_EVENT_HANDLER_ID,
  ):
    probe.set_reg_value(
      rf'SOFTWARE\Classes\CLSID\{ps}\InprocServer32',
      '',
      rf'{install_dir}\remoting_core.dll',
      view=64,
    )
  for iid, expected_ps in audit_host_install.INTERFACE_PROXY_STUBS:
    probe.set_reg_value(
      rf'SOFTWARE\Classes\Interface\{iid}\ProxyStubClsid32',
      '',
      expected_ps,
      view=64,
    )
    probe.set_reg_value(
      rf'SOFTWARE\Classes\Interface\{iid}\TypeLib', '', tlb_id, view=64
    )

  tlb = rf'SOFTWARE\Classes\TypeLib\{tlb_id}\1.0'
  probe.set_reg_value(rf'{tlb}\FLAGS', '', '0', view=64)
  probe.set_reg_value(rf'{tlb}\HELPDIR', '', install_dir, view=64)
  for arch in ('win32', 'win64'):
    probe.set_reg_value(
      rf'{tlb}\0\{arch}', '', rf'{install_dir}\remoting_core.dll', view=64
    )

  probe.set_reg_value(
    r'SOFTWARE\Classes\ChromeRemoteDesktopUrlForwarder\shell\open\command',
    '',
    rf'"{install_dir}\remote_open_url.exe" "%1"',
    view=64,
  )
  probe.set_reg_value(
    r'SOFTWARE\RegisteredApplications',
    'Chrome Remote Desktop URL Forwarder',
    r'SOFTWARE\Google\Chrome Remote Desktop\UrlForwarder\Capabilities',
    view=64,
  )
  root = r'SOFTWARE\Google\Chrome Remote Desktop'
  probe.set_reg_key(
    rf'{root}\UrlForwarder\Capabilities\URLAssociations', view=64
  )
  probe.set_reg_key(rf'{root}\logging', view=64)
  probe.set_reg_key(rf'{root}\paired-clients\clients', view=64)
  probe.set_reg_key(
    rf'{root}\paired-clients\secrets', view=64, sddl=SECRETS_REG_SDDL
  )


class AuditValidatorsTest(unittest.TestCase):
  def test_extract_executable_path_handles_quoted_and_unquoted_commands(
    self,
  ) -> None:
    self.assertEqual(
      audit_host_install.extract_executable_path(
        r'"C:\Program Files\Google\remoting_host.exe" --type=daemon'
      ),
      r'C:\Program Files\Google\remoting_host.exe',
    )
    self.assertEqual(
      audit_host_install.extract_executable_path(
        r'C:\Program Files\Google\remoting_host.exe --type=daemon'
      ),
      r'C:\Program Files\Google\remoting_host.exe',
    )
    self.assertEqual(
      audit_host_install.extract_executable_path(
        r'C:\Chromoting\remoting_host.exe --config=C:\foo.exe'
      ),
      r'C:\Chromoting\remoting_host.exe',
    )
    self.assertEqual(audit_host_install.extract_executable_path('   '), '')

  def test_validate_sddl_accepts_expected_file_reg_and_service_acls(
    self,
  ) -> None:
    self.assertEqual(
      audit_host_install.validate_sddl(
        PROGRAM_FILES_SDDL, object_type='file', require_appcontainer_rx=True
      ),
      [],
    )
    self.assertEqual(
      audit_host_install.validate_sddl(
        CONFIG_DIR_SDDL,
        object_type='file',
        require_protected_dacl=True,
        allow_trusted_installer=False,
      ),
      [],
    )
    self.assertEqual(
      audit_host_install.validate_sddl(
        HOST_JSON_SDDL,
        object_type='file',
        allow_non_admin_read=False,
        allow_trusted_installer=False,
      ),
      [],
    )
    self.assertEqual(
      audit_host_install.validate_sddl(
        UNPRIV_JSON_SDDL, object_type='file', allow_trusted_installer=False
      ),
      [],
    )
    self.assertEqual(
      audit_host_install.validate_sddl(HKLM_DEFAULT_SDDL, object_type='reg'), []
    )
    self.assertEqual(
      audit_host_install.validate_sddl(
        SECRETS_REG_SDDL,
        object_type='reg',
        require_protected_dacl=True,
        allow_non_admin_read=False,
        allow_trusted_installer=False,
      ),
      [],
    )
    self.assertEqual(
      audit_host_install.validate_sddl(
        SERVICE_DEFAULT_SDDL, object_type='service'
      ),
      [],
    )

  def test_install_dir_missing_appcontainer_rx_and_deny_ace_flagged(
    self,
  ) -> None:
    bad_dir_sddl = 'O:BAG:BAD:P(D;;WD;;;BA)(A;OICI;GA;;;SY)(A;OICI;GRGX;;;BU)'
    violations = audit_host_install.validate_sddl(
      bad_dir_sddl, object_type='file', require_appcontainer_rx=True
    )
    joined = '\n'.join(violations)
    self.assertIn('Deny ACE blocks admin/SYSTEM principal BA', joined)
    self.assertIn('Missing AppContainer', joined)

    icacls_sddl = (
      'O:BAG:BAD:P(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)'
      '(A;OICI;GRGX;;;BU)(A;OICI;0x1200a9;;;AC)'
    )
    self.assertEqual(
      audit_host_install.validate_sddl(
        icacls_sddl, object_type='file', require_appcontainer_rx=True
      ),
      [],
    )
    self.assertEqual(
      audit_host_install.validate_sddl(
        LEAF_PAYLOAD_SDDL,
        object_type='file',
        require_appcontainer_rx=True,
        is_container=False,
      ),
      [],
    )
    leaf_no_ac = 'O:BAG:BAD:AI(A;ID;FA;;;SY)(A;ID;FA;;;BA)(A;ID;0x1200a9;;;BU)'
    self.assertEqual(
      audit_host_install.validate_sddl(
        leaf_no_ac,
        object_type='file',
        require_appcontainer_rx=True,
        is_container=False,
      ),
      ['Missing AppContainer (AC / S-1-15-2-1) Read+Execute ACE'],
    )

  def test_sddl_hardening_rejects_delete_child_null_dacl_and_callback_aces(
    self,
  ) -> None:
    dt_sddl = 'O:BAG:BAD:P(A;OICI;GA;;;SY)(A;OICI;CCDT;;;BU)'
    self.assertTrue(
      any(
        'write/control rights' in v
        for v in audit_host_install.validate_sddl(dt_sddl, object_type='file')
      )
    )
    null_dacl = 'O:BAG:BAD:NO_ACCESS_CONTROLS:(ML;;NX;;;ME)'
    self.assertTrue(
      any(
        'Missing or empty DACL' in v
        for v in audit_host_install.validate_sddl(null_dacl)
      )
    )
    xa_and_ac_deny = (
      'O:BAG:BAD:P(A;OICI;GA;;;SY)(D;OICI;GRGX;;;AC)'
      '(A;OICI;CCWP;;;AC)(XA;;GA;;;BU;(1==1))'
    )
    joined = '\n'.join(
      audit_host_install.validate_sddl(
        xa_and_ac_deny, object_type='file', require_appcontainer_rx=True
      )
    )
    self.assertIn('Deny ACE blocks AppContainer principal AC', joined)
    self.assertIn('Unsupported ACE type XA', joined)

    # Trailing ACE after nested-parenthesis conditional ACE must not be skipped.
    nested_tail = 'O:BAG:BAD:P(A;OICI;GA;;;SY)(XD;;GR;;;BU;(1==1))(A;;FA;;;WD)'
    nested_joined = '\n'.join(audit_host_install.validate_sddl(nested_tail))
    self.assertIn(
      'Non-admin principal WD has write/control rights (FA)', nested_joined
    )

    quoted_cond = (
      'O:BAG:BAD:P(XA;;GA;;;BU;(@User.Dept == "S:Admin"))(A;;FA;;;WD)'
    )
    quoted_joined = '\n'.join(audit_host_install.validate_sddl(quoted_cond))
    self.assertIn('Unsupported ACE type XA', quoted_joined)
    self.assertIn(
      'Non-admin principal WD has write/control rights (FA)', quoted_joined
    )

    garbled_tail = 'O:BAG:BAD:P(A;;GA;;;SY)GARBAGE(A;;FA;;;WD)'
    self.assertIn(
      'Unexpected trailing ACL text',
      '\n'.join(audit_host_install.validate_sddl(garbled_tail)),
    )
    dup_dacl = 'O:BAG:BAD:P(A;;GA;;;SY)D:(A;;FA;;;WD)'
    self.assertIn(
      'Duplicate D: section',
      '\n'.join(audit_host_install.validate_sddl(dup_dacl)),
    )

  def test_validate_com_permissions_and_unavailable_acl_fail_closed(
    self,
  ) -> None:
    self.assertEqual(
      audit_host_install.validate_com_permissions_sddl(VALID_COM_SDDL), []
    )
    hex_com_sddl = (
      'O:BAG:BAD:(A;;0xb;;;SY)(A;;0xb;;;BA)(A;;0xb;;;LS)S:(ML;;NX;;;ME)'
    )
    self.assertEqual(
      audit_host_install.validate_com_permissions_sddl(hex_com_sddl), []
    )
    bad_com = (
      'O:BAG:BAD:(A;;CCDCLCSW;;;SY)(A;;CCDC;;;BA)(A;IO;CCDCSW;;;LS)'
      '(XA;;0xb;;;SY;(1==1))(A;;CCDCSW;;;WD)S:(ML;;NX;;;ME)'
    )
    com_joined = '\n'.join(
      audit_host_install.validate_com_permissions_sddl(bad_com)
    )
    self.assertIn('Unexpected COM rights CCDCLCSW for SY', com_joined)
    self.assertIn('Unexpected COM rights CCDC for BA', com_joined)
    self.assertIn('Unexpected COM ACE', com_joined)
    self.assertIn('Missing COM permissions for LS', com_joined)
    self.assertIn('Unexpected COM trustee WD', com_joined)

    win_probe = audit_host_install.WindowsSystemProbe.__new__(
      audit_host_install.WindowsSystemProbe
    )
    for bad_input in (None, 'corrupted', 123, [1, 2], bytearray(24), b'x' * 19):
      self.assertIsNone(win_probe.binary_sd_to_sddl(bad_input))

    entry = audit_host_install.InventoryEntry(
      'Filesystem', r'C:\test', 'exe', True
    )
    audit_host_install._evaluate_acl(entry, None)
    self.assertEqual(entry.acl_status, 'UNAVAILABLE')
    self.assertIn('Security descriptor unavailable', entry.issues[0])


class AuditHostInstallTest(unittest.TestCase):
  def test_clean_machine_has_zero_present_and_zero_issues(self) -> None:
    probe = FakeSystemProbe()
    report = audit_host_install.audit_host_install(probe)
    self.assertEqual(report.present_count, 0)
    self.assertEqual(report.issue_count, 0)
    self.assertEqual(report.acl_invalid_count, 0)
    self.assertEqual(report.detected_branding, [])
    with mock.patch('sys.stdout', new_callable=io.StringIO) as out:
      rc = audit_host_install.main(['--all'], probe=probe)
    self.assertEqual(rc, 0)
    self.assertIn('[ABSENT ]', out.getvalue())

  def test_complete_native_install_passes_all_inventory_and_acl_checks(
    self,
  ) -> None:
    probe = FakeSystemProbe()
    _populate_complete_native_install(probe)
    install_dir = r'C:\Program Files\Google\Chrome Remote Desktop'
    runtime_dir_sddl = (
      'O:SYG:SYD:AI(A;OICIIOID;GA;;;CO)(A;OICIID;FA;;;SY)'
      '(A;OICIID;FA;;;BA)(A;OICIID;0x1200a9;;;BU)'
    )
    probe.add_dir(rf'{install_dir}\minidumps', sddl=runtime_dir_sddl)
    probe.add_dir(rf'{install_dir}\crashpad', sddl=runtime_dir_sddl)
    probe.add_file(
      rf'{install_dir}\chrome_remote_desktop_20261106_140000_000.log',
      sddl=LEAF_PAYLOAD_SDDL,
    )
    probe.add_file(
      rf'{install_dir}\latest.log', sddl=LEAF_PAYLOAD_SDDL, reparse=True
    )
    report = audit_host_install.audit_host_install(probe)
    self.assertEqual(report.detected_branding, ['chrome'])
    self.assertEqual(
      report.issue_count, 0, msg=audit_host_install.format_report(report)
    )
    self.assertEqual(report.acl_invalid_count, 0)
    # 43 OK ACLs = 4 filesystem/ProgramData (install_dir, config_dir, host.json,
    # host_unprivileged.json) + 2 service (SCM + Services\chromoting) + 21
    # non-COM registry (2 Omaha, 10 NMH, EventLog, SecurityKey, UrlForwarder
    # cmd, RegisteredApplications, root + 4 branding subkeys) + 16 COM (AppId,
    # AccessPermission, LaunchPermission, CLSID, LocalServer32, 2 ProxyStub
    # CLSIDs, 2 InprocServer32s, 4 Interfaces, TypeLib 1.0, win32, win64).
    self.assertEqual(report.acl_ok_count, 43)
    with mock.patch('sys.stdout', new_callable=io.StringIO):
      self.assertEqual(audit_host_install.main([], probe=probe), 0)

  def test_legacy_msi_locations_and_firewall_rules_are_inventoried(
    self,
  ) -> None:
    probe = FakeSystemProbe()
    msi_dir = r'C:\Program Files (x86)\Google\Chrome Remote Desktop'
    probe.add_dir(rf'{msi_dir}\145.0.7600.0', sddl=PROGRAM_FILES_SDDL)
    probe.set_file_sddl(msi_dir, PROGRAM_FILES_SDDL)
    probe.msi_products.append('{AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE}')
    probe.set_reg_value(
      r'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall'
      r'\{aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee}',
      'DisplayName',
      'Legacy MSI Product',
      view=64,
    )
    probe.set_reg_value(
      r'SYSTEM\CurrentControlSet\Services\SharedAccess\Parameters'
      r'\FirewallPolicy\FirewallRules',
      '{RULE-1}',
      rf'v2.30|Action=Allow|App={msi_dir}\145.0.7600.0\remoting_network.exe|',
      view=64,
    )
    probe.set_reg_key(
      rf'SOFTWARE\Google\Sawbuck\Providers'
      rf'\{audit_host_install.LEGACY_SAWBUCK_PROVIDER_GUID}',
      view=32,
    )

    report = audit_host_install.audit_host_install(probe)
    msi_present = [e for e in report.entries if e.present and e.source == 'msi']
    categories = {e.category for e in msi_present}
    self.assertEqual(categories, {'Filesystem', 'MSI', 'Firewall', 'Registry'})
    self.assertEqual(report.issue_count, 0)

  def test_programdata_acl_and_reparse_violations_detected(self) -> None:
    probe = FakeSystemProbe()
    cfg_dir = r'C:\ProgramData\Google\Chrome Remote Desktop'
    probe.add_dir(
      cfg_dir,
      sddl='O:S-1-5-21-1000-2000-3000-1001G:BAD:AI(A;OICI;FA;;;BU)',
    )
    probe.add_file(
      rf'{cfg_dir}\host.json',
      sddl='O:BAG:BAD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;AU)',
    )
    probe.add_file(
      rf'{cfg_dir}\temp.json~',
      sddl=HOST_JSON_SDDL,
      reparse=True,
    )

    report = audit_host_install.audit_host_install(probe)
    self.assertGreaterEqual(report.acl_invalid_count, 2)
    all_issues = '\n'.join(i for e in report.entries for i in e.issues)
    self.assertIn('Untrusted owner S-1-5-21-1000-2000-3000-1001', all_issues)
    self.assertIn('Unprotected DACL', all_issues)
    self.assertIn('Non-admin principal BU has write/control rights', all_issues)
    self.assertIn('Non-admin principal AU has access', all_issues)
    self.assertIn('reparse point/symlink', all_issues)
    self.assertIn('.json~', all_issues)

  def test_pairing_secrets_registry_acl_requires_protected_admin_only(
    self,
  ) -> None:
    probe = FakeSystemProbe()
    _populate_complete_native_install(probe)
    probe.set_reg_key(
      r'SOFTWARE\Google\Chrome Remote Desktop\paired-clients\secrets',
      view=64,
      sddl=HKLM_DEFAULT_SDDL,
    )
    report = audit_host_install.audit_host_install(probe)
    self.assertEqual(report.acl_invalid_count, 1)
    all_issues = '\n'.join(i for e in report.entries for i in e.issues)
    self.assertIn('Unprotected DACL', all_issues)
    self.assertIn('Non-admin principal BU has access', all_issues)

  def test_service_acl_and_eventlog_category_violations_detected(self) -> None:
    probe = FakeSystemProbe()
    _populate_complete_native_install(probe)
    probe.service_sddl = 'O:SYG:SYD:(A;;GA;;;SY)(A;;GA;;;BA)(A;;CCDCRP;;;IU)'
    probe.set_reg_value(
      r'SYSTEM\CurrentControlSet\Services\EventLog\Application\chromoting',
      'CategoryMessageFile',
      r'C:\missing_core.dll',
      view=64,
    )

    report = audit_host_install.audit_host_install(probe)
    self.assertEqual(report.acl_invalid_count, 1)
    all_issues = '\n'.join(i for e in report.entries for i in e.issues)
    self.assertIn('Non-admin principal IU has write/control rights', all_issues)
    self.assertIn(
      'CategoryMessageFile: Target file missing or invalid on disk', all_issues
    )

  def test_com_appid_permissions_violations_detected(self) -> None:
    probe = FakeSystemProbe()
    _populate_complete_native_install(probe)
    bad_com = probe.register_binary_sd(
      b'\x01\x00\x04\x80' + b'\x00' * 16 + b'bad',
      'O:BAG:BAD:(A;;0xb;;;SY)(A;;0xb;;;BA)(A;;0xb;;;WD)',
    )
    app_id = rf'SOFTWARE\Classes\AppId\{audit_host_install.RDP_APP_ID}'
    probe.set_reg_value(app_id, 'AccessPermission', 'corrupted_str', view=64)
    probe.set_reg_value(app_id, 'LaunchPermission', bad_com, view=64)
    probe.set_reg_value(app_id, 'RunAs', 'Interactive User', view=64)

    report = audit_host_install.audit_host_install(probe)
    self.assertEqual(report.acl_invalid_count, 2)
    all_issues = '\n'.join(i for e in report.entries for i in e.issues)
    self.assertIn('Unexpected AppId RunAs', all_issues)
    self.assertIn('Invalid binary SD in AccessPermission', all_issues)
    self.assertIn('Unexpected COM trustee WD', all_issues)
    self.assertIn('Missing COM permissions for LS', all_issues)
    self.assertIn('Missing Medium IL no-execute-up label', all_issues)

  def test_dangling_targets_old_files_and_json_cli_output(self) -> None:
    probe = FakeSystemProbe()
    _populate_complete_native_install(probe)
    install_dir = r'C:\Program Files\Google\Chrome Remote Desktop'
    probe.add_file(
      rf'{install_dir}\remoting_host.exe.OLD',
      sddl=LEAF_PAYLOAD_SDDL,
      reparse=True,
    )
    probe.set_reg_value(
      r'SOFTWARE\Google\SecurityKey',
      'RemoteSecurityKeyPath',
      rf'{install_dir}\missing_binary.exe',
      view=64,
    )
    probe.set_reg_value(
      r'SYSTEM\CurrentControlSet\Control\Session Manager',
      'PendingFileRenameOperations',
      [rf'\??\{install_dir}\remoting_host.exe.OLD', ''],
      view=64,
    )

    with mock.patch('sys.stdout', new_callable=io.StringIO) as out:
      rc = audit_host_install.main(['--json'], probe=probe)
    self.assertEqual(rc, 1)
    payload = json.loads(out.getvalue())
    self.assertEqual(payload['issue_count'], 3)
    all_issues = '\n'.join(i for e in payload['entries'] for i in e['issues'])
    self.assertIn(
      'Unexpected reparse point/symlink: remoting_host.exe.OLD', all_issues
    )
    self.assertIn(
      'Staged .old binary pending cleanup: remoting_host.exe.OLD', all_issues
    )
    reboot_entries = [
      e for e in payload['entries'] if e['category'] == 'RebootPending'
    ]
    self.assertTrue(reboot_entries[0]['present'])

  def test_space_free_cmd_with_exe_arg_and_dir_vs_file_and_payload_acls(
    self,
  ) -> None:
    probe = FakeSystemProbe()
    _populate_complete_native_install(probe)
    install_dir = r'C:\Program Files\Google\Chrome Remote Desktop'
    cfg_dir = r'C:\ProgramData\Google\Chrome Remote Desktop'

    # Unquoted space-free command with .exe flag argument must not be flagged
    # as an unquoted path with spaces.
    probe.add_file(r'C:\Chromoting\remoting_host.exe', sddl=LEAF_PAYLOAD_SDDL)
    probe.service.binary_path = (
      r'C:\Chromoting\remoting_host.exe --config=C:\Program Files\foo.exe'
    )

    # Planted sideload DLL, directory impersonating a payload binary, and
    # payload binary missing AppContainer Read+Execute must all be flagged and
    # propagate acl_status == 'INVALID' to the install directory entry.
    probe.add_file(rf'{install_dir}\version.dll', sddl=LEAF_PAYLOAD_SDDL)
    probe.remove_file(rf'{install_dir}\remoting_desktop.exe')
    probe.add_dir(
      rf'{install_dir}\remoting_desktop.exe', sddl=PROGRAM_FILES_SDDL
    )
    probe.set_file_sddl(
      rf'{install_dir}\remoting_core.dll',
      'O:BAG:BAD:AI(A;ID;FA;;;SY)(A;ID;FA;;;BA)(A;ID;0x1200a9;;;BU)',
    )

    # Directory named host.json in ProgramData and regular file at Chromium
    # install_dir / ProgramData location must be flagged.
    probe.remove_file(rf'{cfg_dir}\host.json')
    probe.add_dir(rf'{cfg_dir}\host.json', sddl=HOST_JSON_SDDL)
    probe.add_file(r'C:\Program Files\Chromoting', sddl=LEAF_PAYLOAD_SDDL)
    probe.add_file(r'C:\ProgramData\Chromoting', sddl=CONFIG_DIR_SDDL)

    report = audit_host_install.audit_host_install(probe)
    all_issues = '\n'.join(i for e in report.entries for i in e.issues)
    self.assertNotIn('Unquoted command path with spaces', all_issues)
    self.assertNotIn('Target file missing', all_issues)
    self.assertIn(
      'Unexpected file/directory in install folder: version.dll', all_issues
    )
    self.assertIn('Missing 1 payload file(s): remoting_desktop.exe', all_issues)
    self.assertIn(
      'Unexpected file/directory in install folder: remoting_desktop.exe',
      all_issues,
    )
    self.assertIn(
      'Payload ACL (remoting_core.dll): Missing AppContainer '
      '(AC / S-1-15-2-1) Read+Execute ACE',
      all_issues,
    )
    pf_entry = next(e for e in report.entries if e.location == install_dir)
    self.assertEqual(pf_entry.acl_status, 'INVALID')
    self.assertGreaterEqual(report.acl_invalid_count, 1)
    self.assertIn('Unexpected file/directory in ProgramData folder', all_issues)
    self.assertIn('Install path is not a directory', all_issues)
    self.assertIn('ProgramData config location is not a directory', all_issues)

  def test_runtime_artifacts_reparse_attacks_and_post_uninstall_leftovers(
    self,
  ) -> None:
    probe = FakeSystemProbe()
    _populate_complete_native_install(probe)
    install_dir = r'C:\Program Files\Google\Chrome Remote Desktop'

    # On an active host (present_n > 0), a minidumps junction, a non-latest.log
    # symlink, and a regular file named crashpad must still be flagged.
    probe.add_dir(
      rf'{install_dir}\minidumps', sddl=PROGRAM_FILES_SDDL, reparse=True
    )
    probe.add_file(
      rf'{install_dir}\evil.log', sddl=LEAF_PAYLOAD_SDDL, reparse=True
    )
    probe.add_file(rf'{install_dir}\crashpad', sddl=LEAF_PAYLOAD_SDDL)

    # On an uninstalled host folder (present_n == 0), leftover minidumps,
    # crashpad, and .log entries must be flagged as leftover runtime artifacts.
    leftover_dir = r'C:\Program Files\Chromoting'
    probe.add_dir(leftover_dir, sddl=PROGRAM_FILES_SDDL)
    probe.add_dir(rf'{leftover_dir}\minidumps', sddl=PROGRAM_FILES_SDDL)
    probe.add_dir(rf'{leftover_dir}\crashpad', sddl=PROGRAM_FILES_SDDL)
    probe.add_file(rf'{leftover_dir}\host.log', sddl=LEAF_PAYLOAD_SDDL)

    report = audit_host_install.audit_host_install(probe)
    chrome_entry = next(e for e in report.entries if e.location == install_dir)
    chrome_issues = '\n'.join(chrome_entry.issues)
    self.assertIn('Unexpected reparse point/symlink: minidumps', chrome_issues)
    self.assertIn('Unexpected reparse point/symlink: evil.log', chrome_issues)
    self.assertIn(
      'Unexpected file/directory in install folder: crashpad', chrome_issues
    )
    self.assertNotIn('Leftover runtime artifact', chrome_issues)

    chromium_entry = next(
      e for e in report.entries if e.location == leftover_dir
    )
    chromium_issues = '\n'.join(chromium_entry.issues)
    self.assertIn('Leftover runtime artifact: minidumps', chromium_issues)
    self.assertIn('Leftover runtime artifact: crashpad', chromium_issues)
    self.assertIn('Leftover runtime artifact: host.log', chromium_issues)

  def test_ancestor_reparse_unquoted_cmd_clsid_appid_and_unavailable_acl(
    self,
  ) -> None:
    probe = FakeSystemProbe()
    _populate_complete_native_install(probe)
    probe.add_reparse_point(r'C:\ProgramData\Google')
    probe.service.account = ''
    probe.service.binary_path = (
      r'C:\Program Files\Google\Chrome Remote Desktop\remoting_host.exe '
      r'--type=daemon'
    )
    rdp_clsid = audit_host_install.compute_rdp_desktop_session_clsid(
      DEFAULT_NATIVE_VERSION
    )
    probe.set_reg_value(
      rf'SOFTWARE\Classes\CLSID\{rdp_clsid}', 'AppID', '', view=64
    )
    probe.remove_file_sddl(
      r'C:\ProgramData\Google\Chrome Remote Desktop\host.json'
    )

    report = audit_host_install.audit_host_install(probe)
    all_issues = '\n'.join(i for e in report.entries for i in e.issues)
    self.assertIn('Path or ancestor is a reparse point/symlink', all_issues)
    self.assertIn('Unquoted command path with spaces', all_issues)
    self.assertNotIn('Target file missing', all_issues)
    self.assertIn("Unexpected service account: ''", all_issues)
    self.assertIn('Mismatched CLSID AppID', all_issues)
    self.assertIn('Security descriptor unavailable', all_issues)
    with mock.patch('sys.stdout', new_callable=io.StringIO):
      self.assertEqual(audit_host_install.main([], probe=probe), 1)

  def test_orphaned_multiple_clsids_missing_localserver32_and_proxystub(
    self,
  ) -> None:
    probe = FakeSystemProbe()
    _populate_complete_native_install(probe)

    # Uppercase orphaned CLSID discovered via CLSID enumeration (also tests
    # case-insensitive sorting against the lowercase salted CLSID).
    orphan_clsid = '{FFFFFFFF-2222-3333-4444-555555555555}'
    probe.set_reg_value(
      rf'SOFTWARE\Classes\CLSID\{orphan_clsid}',
      'AppID',
      audit_host_install.RDP_APP_ID,
      view=64,
    )

    # Missing AccessPermission binary SD on AppId attaches issue to `pentry`.
    app_id = rf'SOFTWARE\Classes\AppId\{audit_host_install.RDP_APP_ID}'
    probe.remove_reg_value(app_id, 'AccessPermission', view=64)

    # Wrong ProxyStubClsid32 and Interface TypeLib targets, and bad TypeLib
    # FLAGS/HELPDIR.
    daemon_iid = audit_host_install.IDAEMON_CONTROL_ID
    probe.set_reg_value(
      rf'SOFTWARE\Classes\Interface\{daemon_iid}\ProxyStubClsid32',
      '',
      '{deadbeef-0000-0000-0000-000000000000}',
      view=64,
    )
    probe.set_reg_value(
      rf'SOFTWARE\Classes\Interface\{daemon_iid}\TypeLib',
      '',
      '{deadbeef-0000-0000-0000-000000000000}',
      view=64,
    )
    tlb = (
      rf'SOFTWARE\Classes\TypeLib\{audit_host_install.CHROMOTING_TYPELIB_ID}'
      r'\1.0'
    )
    probe.set_reg_value(rf'{tlb}\FLAGS', '', '9', view=64)
    probe.set_reg_value(rf'{tlb}\HELPDIR', '', r'C:\NoSuchDir', view=64)

    report = audit_host_install.audit_host_install(probe)
    issues_list = [i for e in report.entries for i in e.issues]
    all_issues = '\n'.join(issues_list)
    salted_clsid = audit_host_install.compute_rdp_desktop_session_clsid(
      DEFAULT_NATIVE_VERSION
    )
    salted_entry = next(e for e in report.entries if salted_clsid in e.location)
    self.assertIn(
      'Multiple (2) RdpDesktopSession CLSIDs in 64-bit view',
      '\n'.join(salted_entry.issues),
    )
    self.assertEqual(
      sum(
        1
        for i in issues_list
        if 'Multiple (2) RdpDesktopSession CLSIDs in 64-bit view' in i
      ),
      1,
    )
    self.assertIn(
      r'Missing LocalServer32 subkey under SOFTWARE\Classes\CLSID'
      rf'\{orphan_clsid}',
      all_issues,
    )
    self.assertIn('Missing CLSID TypeLib subkey', all_issues)
    access_entry = next(
      e
      for e in report.entries
      if e.location.endswith(r'\AccessPermission [64-bit]')
    )
    self.assertFalse(access_entry.present)
    self.assertIn(
      'Missing AccessPermission binary value on AppId', access_entry.issues
    )
    self.assertIn('Unexpected ProxyStubClsid32', all_issues)
    self.assertIn('Unexpected Interface TypeLib', all_issues)
    self.assertIn('Unexpected TypeLib FLAGS', all_issues)
    self.assertIn('TypeLib HELPDIR missing or not a directory', all_issues)

  def test_wow64_32bit_entries_and_omaha_pv_validation(self) -> None:
    # 1. Valid 32-bit legacy MSI registry + COM entries with matching 32-bit
    # Omaha `pv`.
    probe = FakeSystemProbe()
    msi_ver = '145.0.7600.0'
    msi_bin_dir = (
      rf'C:\Program Files (x86)\Google\Chrome Remote Desktop\{msi_ver}'
    )
    probe.add_dir(
      r'C:\Program Files (x86)\Google\Chrome Remote Desktop',
      sddl=PROGRAM_FILES_SDDL,
    )
    probe.add_dir(msi_bin_dir, sddl=PROGRAM_FILES_SDDL)
    for fname in (
      'remoting_core.dll',
      'remoting_curtain.exe',
      'remote_security_key.exe',
    ):
      probe.add_file(rf'{msi_bin_dir}\{fname}', sddl=LEAF_PAYLOAD_SDDL)

    omaha = rf'SOFTWARE\Google\Update\Clients\{audit_host_install.OMAHA_APP_ID}'
    probe.set_reg_value(omaha, 'pv', msi_ver, view=32)
    probe.set_reg_value(omaha, 'name', 'Chrome Remote Desktop Host', view=32)
    probe.set_reg_value(
      r'SOFTWARE\Google\SecurityKey',
      'RemoteSecurityKeyPath',
      rf'{msi_bin_dir}\remote_security_key.exe',
      view=32,
    )
    probe.set_reg_value(
      r'SOFTWARE\RegisteredApplications',
      'Chrome Remote Desktop URL Forwarder',
      r'SOFTWARE\Google\Chrome Remote Desktop\UrlForwarder\Capabilities',
      view=32,
    )
    probe.set_reg_key(
      r'SOFTWARE\Google\Chrome Remote Desktop\paired-clients\secrets',
      view=32,
      sddl=SECRETS_REG_SDDL,
    )

    com_sd = probe.register_binary_sd(
      b'\x01\x00\x04\x80' + b'\x00' * 16 + b'msi', VALID_COM_SDDL
    )
    app_id = rf'SOFTWARE\Classes\AppId\{audit_host_install.RDP_APP_ID}'
    probe.set_reg_value(app_id, 'RunAs', audit_host_install.RDP_RUN_AS, view=64)
    probe.set_reg_value(app_id, 'AccessPermission', com_sd, view=64)
    probe.set_reg_value(app_id, 'LaunchPermission', com_sd, view=64)
    msi_clsid = audit_host_install.compute_rdp_desktop_session_clsid(msi_ver)
    probe.set_reg_value(
      rf'SOFTWARE\Classes\CLSID\{msi_clsid}',
      'AppID',
      audit_host_install.RDP_APP_ID,
      view=32,
    )
    probe.set_reg_value(
      rf'SOFTWARE\Classes\CLSID\{msi_clsid}\LocalServer32',
      '',
      rf'"{msi_bin_dir}\remoting_curtain.exe" --type=rdp_desktop_session',
      view=32,
    )
    tlb_id = audit_host_install.CHROMOTING_TYPELIB_ID
    probe.set_reg_value(
      rf'SOFTWARE\Classes\CLSID\{msi_clsid}\TypeLib', '', tlb_id, view=32
    )
    for ps in (
      audit_host_install.IRDP_DESKTOP_SESSION_ID,
      audit_host_install.IRDP_DESKTOP_SESSION_EVENT_HANDLER_ID,
    ):
      probe.set_reg_value(
        rf'SOFTWARE\Classes\CLSID\{ps}\InprocServer32',
        '',
        rf'{msi_bin_dir}\remoting_core.dll',
        view=32,
      )
    for iid, expected_ps in audit_host_install.INTERFACE_PROXY_STUBS:
      probe.set_reg_value(
        rf'SOFTWARE\Classes\Interface\{iid}\ProxyStubClsid32',
        '',
        expected_ps.upper(),
        view=32,
      )
      if iid != audit_host_install.IRDP_DESKTOP_SESSION_ID:
        probe.set_reg_value(
          rf'SOFTWARE\Classes\Interface\{iid}\TypeLib', '', tlb_id, view=32
        )
    tlb = rf'SOFTWARE\Classes\TypeLib\{tlb_id}\1.0'
    probe.set_reg_value(rf'{tlb}\FLAGS', '', '0', view=64)
    probe.set_reg_value(rf'{tlb}\HELPDIR', '', msi_bin_dir + '\\', view=64)
    probe.set_reg_value(
      rf'{tlb}\0\win32', '', rf'{msi_bin_dir}\remoting_core.dll', view=64
    )

    msi_report = audit_host_install.audit_host_install(probe)
    self.assertEqual(
      msi_report.issue_count,
      0,
      msg=audit_host_install.format_report(msi_report),
    )
    present_32 = [
      e for e in msi_report.entries if e.present and '[32-bit]' in e.location
    ]
    self.assertEqual(len(present_32), 15)

    # 2. MSI layout: unmatched 32-bit `pv`, matching versioned dir with missing
    # 32-bit salted CLSID, and leftover `TypeLib\0\win64` without 64-bit server
    # are all flagged.
    probe.set_reg_value(omaha, 'pv', '999.0.0.1', view=32)
    msi_bad_pv = audit_host_install.audit_host_install(probe)
    msi_bad_issues = '\n'.join(i for e in msi_bad_pv.entries for i in e.issues)
    self.assertIn(
      "Omaha pv '999.0.0.1' does not match any installed host version",
      msi_bad_issues,
    )

    probe.set_reg_value(omaha, 'pv', msi_ver, view=32)
    probe.remove_reg_key(rf'SOFTWARE\Classes\CLSID\{msi_clsid}', view=32)
    probe.remove_reg_key(
      rf'SOFTWARE\Classes\CLSID\{msi_clsid}\LocalServer32', view=32
    )
    probe.remove_reg_key(
      rf'SOFTWARE\Classes\CLSID\{msi_clsid}\TypeLib', view=32
    )
    native_dll = (
      r'C:\Program Files\Google\Chrome Remote Desktop\remoting_core.dll'
    )
    probe.add_file(native_dll, sddl=LEAF_PAYLOAD_SDDL)
    probe.set_reg_value(rf'{tlb}\0\win64', '', native_dll, view=64)
    msi_no_clsid = audit_host_install.audit_host_install(probe)
    msi_no_clsid_issues = '\n'.join(
      i for e in msi_no_clsid.entries for i in e.issues
    )
    self.assertIn(
      f'Omaha pv {msi_ver!r} does not match any installed host version',
      msi_no_clsid_issues,
    )
    self.assertIn(
      'Orphaned TypeLib win64 registration without 64-bit COM server',
      msi_no_clsid_issues,
    )

    # 3. Omaha `pv` contributing a version not otherwise discovered (no
    # versioned dir, no `--version` flag, and CLSID `AppID` wiped so
    # enumeration skips it), plus malformed, unmatched, and missing-mirror `pv`
    # values flagged.
    probe2 = FakeSystemProbe()
    omaha_only_ver = '148.0.7800.0'
    _populate_complete_native_install(probe2, version=omaha_only_ver)
    salted_148 = audit_host_install.compute_rdp_desktop_session_clsid(
      omaha_only_ver
    )
    probe2.set_reg_value(
      rf'SOFTWARE\Classes\CLSID\{salted_148}', 'AppID', '', view=64
    )
    probe2.set_reg_value(omaha, 'pv', '999.0.0.1', view=32)

    report2 = audit_host_install.audit_host_install(probe2)
    issues2 = '\n'.join(i for e in report2.entries for i in e.issues)
    self.assertIn('Mismatched CLSID AppID', issues2)
    self.assertIn(
      "Omaha pv '999.0.0.1' does not match any installed host version", issues2
    )
    self.assertIn(
      "Omaha 32-bit pv '999.0.0.1' disagrees with 64-bit pv '148.0.7800.0'",
      issues2,
    )

    probe2.set_reg_value(omaha, 'pv', 'bad-version', view=64)
    report3 = audit_host_install.audit_host_install(probe2)
    issues3 = '\n'.join(i for e in report3.entries for i in e.issues)
    self.assertIn("Invalid or missing Omaha pv: 'bad-version'", issues3)

    probe2.set_reg_value(omaha, 'pv', omaha_only_ver, view=64)
    probe2.remove_reg_key(omaha, view=32)
    report4 = audit_host_install.audit_host_install(probe2)
    issues4 = '\n'.join(i for e in report4.entries for i in e.issues)
    self.assertIn(
      'Missing 32-bit Omaha Client mirror for native Chrome host', issues4
    )

  def test_partial_and_orphaned_com_states_flagged(self) -> None:
    # 1. AppId without CLSID, orphaned Interfaces in both views without a COM
    # server, and orphaned TypeLib without any COM server.
    probe = FakeSystemProbe()
    com_sd = probe.register_binary_sd(
      b'\x01\x00\x04\x80' + b'\x00' * 16 + b'ok', VALID_COM_SDDL
    )
    app_id = rf'SOFTWARE\Classes\AppId\{audit_host_install.RDP_APP_ID}'
    probe.set_reg_value(app_id, 'RunAs', audit_host_install.RDP_RUN_AS, view=64)
    probe.set_reg_value(app_id, 'AccessPermission', com_sd, view=64)
    probe.set_reg_value(app_id, 'LaunchPermission', com_sd, view=64)
    rdp_iid = audit_host_install.IRDP_DESKTOP_SESSION_ID
    for view in (64, 32):
      probe.set_reg_value(
        rf'SOFTWARE\Classes\Interface\{rdp_iid}\ProxyStubClsid32',
        '',
        rdp_iid,
        view=view,
      )

    report = audit_host_install.AuditReport()
    audit_host_install._audit_com_registrations(probe, report, [])
    all_issues = '\n'.join(i for e in report.entries for i in e.issues)
    self.assertIn(
      'AppId is present without any registered RdpDesktopSession CLSID',
      all_issues,
    )
    self.assertIn(
      'Orphaned COM Interface registration in 64-bit view without COM server',
      all_issues,
    )
    self.assertIn(
      'Orphaned COM Interface registration in 32-bit view without COM server',
      all_issues,
    )

    # Orphaned TypeLib registration with neither AppId nor CLSID.
    probe_tlb = FakeSystemProbe()
    tlb = (
      rf'SOFTWARE\Classes\TypeLib\{audit_host_install.CHROMOTING_TYPELIB_ID}'
      r'\1.0'
    )
    probe_tlb.set_reg_value(rf'{tlb}\FLAGS', '', '0', view=64)
    install_dir = r'C:\Program Files\Google\Chrome Remote Desktop'
    probe_tlb.add_dir(install_dir, sddl=PROGRAM_FILES_SDDL)
    probe_tlb.set_reg_value(rf'{tlb}\HELPDIR', '', install_dir, view=64)
    tlb_report = audit_host_install.AuditReport()
    audit_host_install._audit_com_registrations(probe_tlb, tlb_report, [])
    tlb_issues = '\n'.join(i for e in tlb_report.entries for i in e.issues)
    self.assertIn(
      'Orphaned TypeLib registration without COM server', tlb_issues
    )

    # 2. Partial 64-bit COM server: missing AppId, missing ProxyStub, missing
    # Interface, missing TypeLib win32 & win64, plus missing CLSID when only a
    # ProxyStub exists in 32-bit view.
    probe_part = FakeSystemProbe()
    _populate_complete_native_install(probe_part)
    probe_part.remove_reg_key(app_id, view=64)
    probe_part.remove_reg_key(
      rf'SOFTWARE\Classes\CLSID\{rdp_iid}\InprocServer32', view=64
    )
    daemon_iid = audit_host_install.IDAEMON_CONTROL_ID
    probe_part.remove_reg_key(
      rf'SOFTWARE\Classes\Interface\{daemon_iid}', view=64
    )
    probe_part.remove_reg_key(rf'{tlb}\0\win64', view=64)
    probe_part.remove_reg_key(rf'{tlb}\0\win32', view=64)
    msi_dll = (
      r'C:\Program Files (x86)\Google\Chrome Remote Desktop\145.0.7600.0'
      r'\remoting_core.dll'
    )
    probe_part.add_file(msi_dll, sddl=LEAF_PAYLOAD_SDDL)
    probe_part.set_reg_value(
      rf'SOFTWARE\Classes\CLSID\{rdp_iid}\InprocServer32',
      '',
      msi_dll,
      view=32,
    )
    part_report = audit_host_install.audit_host_install(probe_part)
    part_issues = '\n'.join(i for e in part_report.entries for i in e.issues)
    self.assertIn(
      'Missing AppId for registered RdpDesktopSession CLSID', part_issues
    )
    self.assertIn(
      'Missing ProxyStub InprocServer32 in 64-bit view '
      '(partial COM registration)',
      part_issues,
    )
    self.assertIn(
      'Missing COM Interface registration in 64-bit view '
      '(partial COM registration)',
      part_issues,
    )
    self.assertIn(
      'Missing RdpDesktopSession CLSID in 32-bit view '
      '(partial COM registration)',
      part_issues,
    )
    self.assertIn(
      'Missing TypeLib win64 registration for 64-bit COM server', part_issues
    )
    self.assertIn(
      'Missing TypeLib win32 registration for COM server', part_issues
    )

  def test_com_target_identities_and_wow64_leftover_interfaces(self) -> None:
    probe = FakeSystemProbe()
    _populate_complete_native_install(probe)
    install_dir = r'C:\Program Files\Google\Chrome Remote Desktop'
    other_dir = r'C:\OtherApp'
    probe.add_dir(other_dir, sddl=PROGRAM_FILES_SDDL)
    probe.add_file(rf'{other_dir}\remoting_core.dll', sddl=LEAF_PAYLOAD_SDDL)

    rdp_clsid = audit_host_install.compute_rdp_desktop_session_clsid(
      DEFAULT_NATIVE_VERSION
    )
    ls_key = rf'SOFTWARE\Classes\CLSID\{rdp_clsid}\LocalServer32'
    # Existing binary with wrong filename (`remoting_host.exe`).
    probe.set_reg_value(
      ls_key,
      '',
      rf'"{install_dir}\remoting_host.exe" --type=rdp_desktop_session',
      view=64,
    )
    rdp_iid = audit_host_install.IRDP_DESKTOP_SESSION_ID
    probe.set_reg_value(
      rf'SOFTWARE\Classes\CLSID\{rdp_iid}\InprocServer32',
      '',
      rf'{other_dir}\remoting_core.dll',
      view=64,
    )
    tlb = (
      rf'SOFTWARE\Classes\TypeLib\{audit_host_install.CHROMOTING_TYPELIB_ID}'
      r'\1.0'
    )
    probe.set_reg_value(rf'{tlb}\HELPDIR', '', other_dir, view=64)
    probe.set_reg_value(
      rf'{tlb}\0\win64', '', rf'{other_dir}\remoting_core.dll', view=64
    )

    # Leftover 32-bit Interface keys (`IDaemonControl` + `IRdpDesktopSession`)
    # from a partial MSI uninstall alongside a healthy 64-bit native install.
    tlb_id = audit_host_install.CHROMOTING_TYPELIB_ID
    daemon_iid = audit_host_install.IDAEMON_CONTROL_ID
    probe.set_reg_value(
      rf'SOFTWARE\Classes\Interface\{daemon_iid}\ProxyStubClsid32',
      '',
      audit_host_install.UNIVERSAL_MARSHALER_CLSID,
      view=32,
    )
    probe.set_reg_value(
      rf'SOFTWARE\Classes\Interface\{daemon_iid}\TypeLib', '', tlb_id, view=32
    )
    probe.set_reg_value(
      rf'SOFTWARE\Classes\Interface\{rdp_iid}\ProxyStubClsid32',
      '',
      rdp_iid,
      view=32,
    )

    report = audit_host_install.audit_host_install(probe)
    issues_list = [i for e in report.entries for i in e.issues]
    all_issues = '\n'.join(issues_list)
    self.assertIn('Unexpected LocalServer32 binary', all_issues)
    self.assertIn('Unexpected InprocServer32 binary', all_issues)
    self.assertIn('Unexpected TypeLib HELPDIR', all_issues)
    self.assertIn('Unexpected TypeLib win64 binary', all_issues)
    self.assertEqual(
      sum(
        1
        for i in issues_list
        if 'Orphaned COM Interface registration in 32-bit view' in i
      ),
      2,
    )

    # Correct binary (`remoting_curtain.exe`) with wrong command-line flags,
    # ProxyStub root SDDL & ThreadingModel checks, Interface detail with missing
    # ProxyStubClsid32, and CLSID enumeration OSError reporting.
    probe.set_reg_value(
      ls_key, '', rf'"{install_dir}\remoting_curtain.exe" --wrong-arg', view=64
    )
    probe.set_reg_key(
      rf'SOFTWARE\Classes\CLSID\{rdp_iid}',
      view=64,
      sddl='O:BAG:BAD:(A;;KA;;;WD)',
    )
    probe.set_reg_value(
      rf'SOFTWARE\Classes\CLSID\{rdp_iid}\InprocServer32',
      'ThreadingModel',
      'Apartment',
      view=64,
    )
    ev_ps = audit_host_install.IRDP_DESKTOP_SESSION_EVENT_HANDLER_ID
    probe.set_reg_value(
      rf'SOFTWARE\Classes\CLSID\{ev_ps}\InprocServer32',
      'ThreadingModel',
      'Both',
      view=64,
    )
    probe.remove_reg_key(
      rf'SOFTWARE\Classes\Interface\{daemon_iid}\ProxyStubClsid32', view=32
    )
    with mock.patch.object(
      probe, 'find_clsids_by_appid', side_effect=PermissionError('access')
    ):
      report2 = audit_host_install.audit_host_install(probe)
    issues2 = '\n'.join(i for e in report2.entries for i in e.issues)
    self.assertIn('Unexpected LocalServer32 command line', issues2)
    self.assertIn(
      "Unexpected InprocServer32 ThreadingModel 'Apartment'", issues2
    )
    self.assertNotIn("ThreadingModel 'Both'", issues2)
    self.assertIn('Non-admin principal WD has write/control rights', issues2)
    self.assertIn('CLSID enumeration failed in 64-bit view', issues2)
    daemon_32 = next(
      e
      for e in report2.entries
      if e.location == rf'HKLM\SOFTWARE\Classes\Interface\{daemon_iid} [32-bit]'
    )
    self.assertEqual(daemon_32.detail, f'TypeLib={tlb_id!r}')


if __name__ == '__main__':
  unittest.main()
