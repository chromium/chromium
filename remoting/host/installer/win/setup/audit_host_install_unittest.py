#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for audit_host_install.py."""

from typing import Any, Optional
import unittest

import audit_host_install

PROGRAM_FILES_SDDL = (
  f'O:{audit_host_install.TRUSTED_INSTALLER_SID}G:BAD:PAI'
  '(A;OICIIO;GA;;;CO)'
  '(A;OICI;GA;;;SY)(A;OICI;GA;;;BA)'
  '(A;OICI;GRGX;;;BU)'
  '(A;OICIIO;GXGR;;;AC)(A;;0x1200a9;;;AC)'
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

  def add_dir(self, path: str, sddl: Optional[str] = None) -> None:
    norm = self._norm(path)
    self.dirs.setdefault(norm, set())
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
        r'C:\Chromoting\remoting_host.exe --type=daemon'
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


if __name__ == '__main__':
  unittest.main()
