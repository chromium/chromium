#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""OS probe and SDDL security descriptor validators for CRD Windows audits."""

from contextlib import contextmanager
import ctypes
from dataclasses import dataclass, field
import os
import re
import sys
from typing import Any, Iterator, Optional

if sys.platform == 'win32':
  import winreg

# Keep SIDs and rights in sync with security_descriptor.h and chromoting.wxs.
ADMIN_SIDS = frozenset({'BA', 'SY', 'S-1-5-32-544', 'S-1-5-18'})
TRUSTED_INSTALLER_SID = (
  'S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464'
)
CREATOR_OWNER_SIDS = frozenset({'CO', 'S-1-3-0'})
APPCONTAINER_SIDS = frozenset({'AC', 'S-1-15-2-1'})
LOCAL_SERVICE_SIDS = frozenset({'LS', 'S-1-5-19'})
MEDIUM_IL_SIDS = frozenset({'ME', 'S-1-16-8192'})
BROAD_DENY_SIDS = ADMIN_SIDS | frozenset({'WD', 'S-1-1-0', 'AU', 'S-1-5-11'})

SAFE_RIGHTS_BY_TYPE: dict[str, tuple[frozenset[str], int]] = {
  'file': (
    frozenset({'GR', 'GX', 'FR', 'FX', 'RC', 'SY', 'CC', 'SW', 'WP', 'LO'}),
    0xA01200A9,
  ),
  'reg': (
    frozenset({'GR', 'GX', 'KR', 'KX', 'RC', 'SY', 'CC', 'SW', 'RP'}),
    0xA0120019,
  ),
  'service': (
    frozenset({'GR', 'RC', 'SY', 'CC', 'LC', 'SW', 'LO', 'CR'}),
    0x8012018D,
  ),
}


@dataclass(frozen=True)
class BrandingSpec:
  name: str
  rel_dir: str
  url_forwarder_name: str
  url_forwarder_prog_id: str


CHROME_BRANDING = BrandingSpec(
  'chrome',
  r'Google\Chrome Remote Desktop',
  'Chrome Remote Desktop URL Forwarder',
  'ChromeRemoteDesktopUrlForwarder',
)
CHROMIUM_BRANDING = BrandingSpec(
  'chromium',
  'Chromoting',
  'Chromoting URL Forwarder',
  'ChromotingUrlForwarder',
)
BRANDING_SPECS = {'chrome': CHROME_BRANDING, 'chromium': CHROMIUM_BRANDING}


@dataclass
class ServiceInfo:
  exists: bool
  state: str = ''
  start_type: str = ''
  account: str = ''
  binary_path: str = ''


@dataclass
class InventoryEntry:
  category: str
  location: str
  source: str  # 'both', 'exe', or 'msi'
  present: bool
  detail: str = ''
  acl_status: str = 'N/A'  # 'OK', 'INVALID', 'UNAVAILABLE', or 'N/A'
  acl_sddl: str = ''
  issues: list[str] = field(default_factory=list)


@dataclass
class AuditReport:
  detected_branding: list[str] = field(default_factory=list)
  present_count: int = 0
  acl_ok_count: int = 0
  acl_invalid_count: int = 0
  issue_count: int = 0
  entries: list[InventoryEntry] = field(default_factory=list)


def extract_executable_path(command_line: str) -> str:
  """Extracts the executable path from a quoted or unquoted command line."""
  cmd = command_line.strip()
  if not cmd:
    return ''
  if cmd.startswith('"') and (end := cmd.find('"', 1)) != -1:
    return cmd[1:end]
  return cmd.split()[0]


def _join_win_path(base: str, *parts: str) -> str:
  return '\\'.join([base.rstrip('\\/')] + [p.strip('\\/') for p in parts])


def _split_sddl_tokens(rights: str) -> Optional[list[str]]:
  if len(rights) % 2 != 0:
    return None
  return [rights[i : i + 2].upper() for i in range(0, len(rights), 2)]


def _has_unsafe_rights(rights_str: str, object_type: str) -> bool:
  rights = rights_str.strip()
  if not rights:
    return False
  safe_tokens, safe_mask = SAFE_RIGHTS_BY_TYPE[object_type]
  if rights.lower().startswith('0x'):
    try:
      return (int(rights, 16) & ~safe_mask) != 0
    except ValueError:
      return True
  tokens = _split_sddl_tokens(rights)
  return tokens is None or any(t not in safe_tokens for t in tokens)


def _grants_file_read_exec(rights_str: str) -> bool:
  rights = rights_str.strip()
  if rights.lower().startswith('0x'):
    try:
      mask = int(rights, 16)
      return (
        (mask & 0x21) == 0x21
        or (mask & 0xA0000000) == 0xA0000000
        or bool(mask & 0x10000000)
      )
    except ValueError:
      return False
  tokens = set(_split_sddl_tokens(rights) or ())
  return (
    {'GR', 'GX'}.issubset(tokens)
    or {'FR', 'FX'}.issubset(tokens)
    or {'CC', 'WP'}.issubset(tokens)
    or bool(tokens & {'FA', 'GA'})
  )


def _parse_sddl_sections(sddl: str) -> tuple[dict[str, str], list[str]]:
  """Splits an SDDL string into top-level O/G/D/S sections."""
  sections: dict[str, str] = {}
  errors: list[str] = []
  cur_key: Optional[str] = None
  start = 0
  depth = 0
  in_quotes = False
  i = 0
  while i < len(sddl):
    ch = sddl[i]
    if depth > 0 and ch == '"':
      in_quotes = not in_quotes
    elif not in_quotes:
      if ch == '(':
        depth += 1
      elif ch == ')':
        if depth == 0:
          errors.append(f'Unbalanced closing parenthesis in SDDL ({sddl})')
        else:
          depth -= 1
      elif depth == 0 and i + 1 < len(sddl) and sddl[i + 1] == ':':
        key = ch.upper()
        if cur_key is not None:
          sections[cur_key] = sddl[start:i]
        elif sddl[:i].strip():
          errors.append(f'Unexpected SDDL prefix {sddl[:i]!r}')
        if key not in {'O', 'G', 'D', 'S'}:
          errors.append(f'Unknown SDDL section {ch}:')
        elif key in sections:
          errors.append(f'Duplicate {key}: section in SDDL')
        cur_key = key
        start = i + 2
        i += 2
        continue
    i += 1
  if depth != 0 or in_quotes:
    errors.append(f'Unclosed parenthesis or quote in SDDL ({sddl})')
  if cur_key is not None:
    sections[cur_key] = sddl[start:]
  elif sddl.strip():
    errors.append(f'Malformed SDDL ({sddl})')
  return sections, errors


def _parse_acl_section(body: str) -> tuple[str, list[str], list[str]]:
  """Parses an SDDL ACL body into (flags, balanced_aces, errors)."""
  first_paren = body.find('(')
  if first_paren == -1:
    return body.strip(), [], []
  flags = body[:first_paren].strip()
  aces: list[str] = []
  errors: list[str] = []
  if flags and not re.fullmatch(r'[A-Za-z_]+', flags):
    errors.append(f'Malformed ACL flags {flags!r}')
  pos = first_paren
  while pos < len(body):
    while pos < len(body) and body[pos].isspace():
      pos += 1
    if pos >= len(body):
      break
    if body[pos] != '(':
      errors.append(f'Unexpected trailing ACL text {body[pos:]!r}')
      break
    depth = 0
    in_quotes = False
    ace_start = pos + 1
    while pos < len(body):
      ch = body[pos]
      if ch == '"':
        in_quotes = not in_quotes
      elif not in_quotes:
        if ch == '(':
          depth += 1
        elif ch == ')':
          depth -= 1
          if depth == 0:
            aces.append(body[ace_start:pos])
            pos += 1
            break
      pos += 1
    else:
      errors.append(f'Unclosed ACE in ACL ({body[ace_start - 1 :]})')
      break
  return flags, aces, errors


def validate_sddl(
  sddl: str,
  object_type: str = 'file',
  require_protected_dacl: bool = False,
  allow_non_admin_read: bool = True,
  allow_trusted_installer: bool = True,
  require_appcontainer_rx: bool = False,
) -> list[str]:
  """Validates an SDDL string for owner, DACL protection, and ACE safety."""
  sections, violations = _parse_sddl_sections(sddl)
  trusted = set(ADMIN_SIDS) | (
    {TRUSTED_INSTALLER_SID} if allow_trusted_installer else set()
  )

  owner = sections.get('O', '').strip().upper()
  if not owner:
    violations.append(f'Missing owner in SDDL ({sddl})')
  elif owner not in trusted:
    violations.append(f'Untrusted owner {owner}')

  raw_dacl_flags, dacl_aces, dacl_errors = _parse_acl_section(
    sections.get('D', '')
  )
  violations.extend(dacl_errors)
  if 'D' not in sections or not dacl_aces:
    return violations + [f'Missing or empty DACL in SDDL ({sddl})']

  dacl_flags = raw_dacl_flags.upper()
  if require_protected_dacl and 'P' not in dacl_flags:
    violations.append(f'Unprotected DACL (missing P flag in D:{dacl_flags})')

  ac_self_rx = ac_inherit_rx = False
  for ace in dacl_aces:
    fields = ace.split(';')
    if len(fields) < 6:
      violations.append(f'Malformed ACE ({ace})')
      continue
    ace_type, ace_flags = fields[0].strip().upper(), fields[1].strip().upper()
    rights, trustee = fields[2].strip(), fields[5].strip().upper()

    if ace_type in {'D', 'OD', 'XD'}:
      if trustee in BROAD_DENY_SIDS:
        violations.append(f'Deny ACE blocks admin/SYSTEM principal {trustee}')
      if require_appcontainer_rx and trustee in (
        APPCONTAINER_SIDS | {'WD', 'S-1-1-0'}
      ):
        violations.append(f'Deny ACE blocks AppContainer principal {trustee}')
      continue
    if ace_type != 'A':
      violations.append(f'Unsupported ACE type {ace_type} ({ace})')
      continue

    if trustee in APPCONTAINER_SIDS and _grants_file_read_exec(rights):
      ac_self_rx = ac_self_rx or ('IO' not in ace_flags)
      ac_inherit_rx = ac_inherit_rx or ('OI' in ace_flags and 'CI' in ace_flags)

    if trustee in trusted:
      continue
    if trustee in CREATOR_OWNER_SIDS:
      if (
        require_protected_dacl
        or not allow_non_admin_read
        or 'IO' not in ace_flags
      ):
        violations.append(f'Unsafe CREATOR OWNER ACE ({ace})')
      continue
    if not allow_non_admin_read:
      violations.append(
        f'Non-admin principal {trustee} has access ({rights}) to restricted '
        'object'
      )
    elif _has_unsafe_rights(rights, object_type):
      violations.append(
        f'Non-admin principal {trustee} has write/control rights ({rights})'
      )

  if require_appcontainer_rx and not (ac_self_rx and ac_inherit_rx):
    violations.append(
      'Missing AppContainer (AC / S-1-15-2-1) inheritable Read+Execute ACE'
    )
  return violations


COM_SDDL_TOKEN_MASKS = {'CC': 0x1, 'DC': 0x2, 'LC': 0x4, 'SW': 0x8}


def _decode_com_rights(rights_str: str) -> int:
  """Decodes a COM ACE rights string (`0x...` or 2-letter SDDL tokens)."""
  rights = rights_str.strip()
  if not rights:
    return -1
  if rights.lower().startswith('0x'):
    try:
      return int(rights, 16)
    except ValueError:
      return -1
  tokens = _split_sddl_tokens(rights)
  if not tokens or any(t not in COM_SDDL_TOKEN_MASKS for t in tokens):
    return -1
  mask = 0
  for token in tokens:
    mask |= COM_SDDL_TOKEN_MASKS[token]
  return mask


def validate_com_permissions_sddl(sddl: str) -> list[str]:
  """Validates RdpDesktopSession COM Launch/AccessPermission SDDL."""
  sections, violations = _parse_sddl_sections(sddl)
  owner = sections.get('O', '').strip().upper()
  if not owner or owner not in ADMIN_SIDS:
    violations.append(f'Untrusted COM SD owner in {sddl}')

  _, dacl_aces, dacl_errors = _parse_acl_section(sections.get('D', ''))
  violations.extend(dacl_errors)
  if 'D' not in sections or not dacl_aces:
    return violations + [f'Missing DACL in COM SD ({sddl})']

  granted: set[str] = set()
  for ace in dacl_aces:
    fields = ace.split(';')
    if len(fields) < 6 or fields[0].strip().upper() != 'A' or fields[1].strip():
      violations.append(f'Unexpected COM ACE ({ace})')
      continue
    rights, trustee = fields[2].strip(), fields[5].strip().upper()
    if _decode_com_rights(rights) != 0xB:
      violations.append(f'Unexpected COM rights {rights} for {trustee}')
    if trustee in {'SY', 'S-1-5-18'}:
      granted.add('SY')
    elif trustee in {'BA', 'S-1-5-32-544'}:
      granted.add('BA')
    elif trustee in LOCAL_SERVICE_SIDS:
      granted.add('LS')
    else:
      violations.append(f'Unexpected COM trustee {trustee}')

  if missing := ({'SY', 'BA', 'LS'} - granted):
    violations.append(
      f'Missing COM permissions for {", ".join(sorted(missing))}'
    )

  _, sacl_aces, sacl_errors = _parse_acl_section(sections.get('S', ''))
  violations.extend(sacl_errors)
  has_nx = any(
    len(f := ace.split(';')) >= 6
    and f[0].strip().upper() == 'ML'
    and f[5].strip().upper() in MEDIUM_IL_SIDS
    and ('NX' in f[2].upper() or f[2].strip().upper() == '0X4')
    for ace in sacl_aces
  )
  if not has_nx:
    violations.append('Missing Medium IL no-execute-up label (ML;;NX;;;ME)')
  return violations


class SystemProbe:
  """Abstract interface for probing OS filesystem, registry, and services."""

  def get_env(self, name: str) -> Optional[str]:
    raise NotImplementedError

  def path_exists(self, path: str) -> bool:
    raise NotImplementedError

  def is_dir(self, path: str) -> bool:
    raise NotImplementedError

  def is_reparse_point(self, path: str) -> bool:
    raise NotImplementedError

  def list_dir(self, path: str) -> list[str]:
    raise NotImplementedError

  def get_file_sddl(self, path: str) -> Optional[str]:
    return None

  def reg_key_exists(self, subkey: str, view: int = 64) -> bool:
    raise NotImplementedError

  def reg_get_value(
    self, subkey: str, value_name: str, view: int = 64
  ) -> Optional[Any]:
    raise NotImplementedError

  def reg_list_subkeys(self, subkey: str, view: int = 64) -> list[str]:
    raise NotImplementedError

  def reg_list_values(self, subkey: str, view: int = 64) -> list[str]:
    raise NotImplementedError

  def get_reg_sddl(self, subkey: str, view: int = 64) -> Optional[str]:
    return None

  def binary_sd_to_sddl(self, raw_sd: Any) -> Optional[str]:
    return None

  def query_service(self, service_name: str) -> ServiceInfo:
    raise NotImplementedError

  def get_service_sddl(self, service_name: str) -> Optional[str]:
    return None

  def enum_msi_products(self, upgrade_code: str) -> list[str]:
    raise NotImplementedError


class _ServiceStatus(ctypes.Structure):
  _fields_ = [
    ('dwServiceType', ctypes.c_uint32),
    ('dwCurrentState', ctypes.c_uint32),
    ('_reserved', ctypes.c_uint32 * 5),
  ]


class WindowsSystemProbe(SystemProbe):
  """Live Windows OS implementation of SystemProbe."""

  def __init__(self) -> None:
    k32 = self._k32 = ctypes.windll.kernel32
    adv = self._adv = ctypes.windll.advapi32
    ntdll = self._ntdll = ctypes.windll.ntdll
    msi = self._msi = ctypes.windll.msi
    vp, wp, u32, ci = (
      ctypes.c_void_p,
      ctypes.c_wchar_p,
      ctypes.c_uint32,
      ctypes.c_int,
    )
    pvp, pss = ctypes.POINTER(vp), ctypes.POINTER(_ServiceStatus)
    sd_to_str = 'ConvertSecurityDescriptorToStringSecurityDescriptorW'
    valid_rel_sd = 'RtlValidRelativeSecurityDescriptor'
    for dll, name, res, args in (
      (k32, 'GetFileAttributesW', u32, [wp]),
      (k32, 'CreateFileW', vp, [wp, u32, u32, vp, u32, u32, vp]),
      (k32, 'CloseHandle', ci, [vp]),
      (k32, 'LocalFree', vp, [vp]),
      (adv, 'GetSecurityInfo', u32, [vp, ci, u32, vp, vp, vp, vp, pvp]),
      (adv, 'GetNamedSecurityInfoW', u32, [wp, ci, u32, vp, vp, vp, vp, pvp]),
      (ntdll, valid_rel_sd, ctypes.c_ubyte, [vp, u32, u32]),
      (adv, sd_to_str, ci, [vp, u32, u32, pvp, vp]),
      (adv, 'OpenSCManagerW', vp, [wp, wp, u32]),
      (adv, 'OpenServiceW', vp, [vp, wp, u32]),
      (adv, 'QueryServiceStatus', ci, [vp, pss]),
      (adv, 'CloseServiceHandle', ci, [vp]),
      (msi, 'MsiEnumRelatedProductsW', u32, [wp, u32, u32, wp]),
    ):
      fn = getattr(dll, name)
      fn.restype, fn.argtypes = res, args

  @contextmanager
  def _open_hklm(self, subkey: str, view: int) -> Iterator[Any]:
    flag = winreg.KEY_WOW64_32KEY if view == 32 else winreg.KEY_WOW64_64KEY
    with winreg.OpenKey(
      winreg.HKEY_LOCAL_MACHINE, subkey, 0, winreg.KEY_READ | flag
    ) as key:
      yield key

  def _sd_ptr_to_sddl(self, sd_ptr: Any, sec_info: int) -> Optional[str]:
    sddl_ptr = ctypes.c_void_p()
    fn = self._adv.ConvertSecurityDescriptorToStringSecurityDescriptorW
    if not fn(sd_ptr, 1, sec_info, ctypes.byref(sddl_ptr), None):
      return None
    if not sddl_ptr.value:
      return None
    try:
      return ctypes.wstring_at(sddl_ptr.value)
    finally:
      self._k32.LocalFree(sddl_ptr)

  def _free_and_format_sd(self, sd_ptr: ctypes.c_void_p) -> Optional[str]:
    if not sd_ptr.value:
      return None
    try:
      return self._sd_ptr_to_sddl(sd_ptr, 0x7)
    finally:
      self._k32.LocalFree(sd_ptr)

  def _handle_sddl(self, handle: Any, obj_type: int) -> Optional[str]:
    sd = ctypes.c_void_p()
    rc = self._adv.GetSecurityInfo(
      handle, obj_type, 0x7, None, None, None, None, ctypes.byref(sd)
    )
    return self._free_and_format_sd(sd) if rc == 0 else None

  def _reg_enum(self, subkey: str, view: int, fn: Any) -> list[str]:
    out: list[str] = []
    try:
      with self._open_hklm(subkey, view) as key:
        while True:
          out.append(fn(key, len(out)))
    except OSError:
      pass
    return sorted(out)

  def get_env(self, name: str) -> Optional[str]:
    return os.environ.get(name)

  def path_exists(self, path: str) -> bool:
    return self._k32.GetFileAttributesW(path) != 0xFFFFFFFF

  def is_dir(self, path: str) -> bool:
    attrs = self._k32.GetFileAttributesW(path)
    return attrs != 0xFFFFFFFF and bool(attrs & 0x10)

  def is_reparse_point(self, path: str) -> bool:
    attrs = self._k32.GetFileAttributesW(path)
    return attrs != 0xFFFFFFFF and bool(attrs & 0x400)

  def list_dir(self, path: str) -> list[str]:
    try:
      return sorted(os.listdir(path))
    except OSError:
      return []

  def get_file_sddl(self, path: str) -> Optional[str]:
    handle = self._k32.CreateFileW(
      path, 0x00020000, 0x7, None, 3, 0x02200000, None
    )
    if not handle or handle == ctypes.c_void_p(-1).value:
      return None
    try:
      return self._handle_sddl(handle, 1)
    finally:
      self._k32.CloseHandle(handle)

  def reg_key_exists(self, subkey: str, view: int = 64) -> bool:
    try:
      with self._open_hklm(subkey, view):
        return True
    except PermissionError:
      return True
    except OSError:
      return False

  def reg_get_value(
    self, subkey: str, value_name: str, view: int = 64
  ) -> Optional[Any]:
    try:
      with self._open_hklm(subkey, view) as key:
        return winreg.QueryValueEx(key, value_name)[0]
    except OSError:
      return None

  def reg_list_subkeys(self, subkey: str, view: int = 64) -> list[str]:
    return self._reg_enum(subkey, view, winreg.EnumKey)

  def reg_list_values(self, subkey: str, view: int = 64) -> list[str]:
    return self._reg_enum(subkey, view, lambda k, i: winreg.EnumValue(k, i)[0])

  def get_reg_sddl(self, subkey: str, view: int = 64) -> Optional[str]:
    try:
      with self._open_hklm(subkey, view) as key:
        return self._handle_sddl(ctypes.c_void_p(int(key)), 4)
    except OSError:
      return None

  def binary_sd_to_sddl(self, raw_sd: Any) -> Optional[str]:
    if not isinstance(raw_sd, bytes) or len(raw_sd) < 20:
      return None
    buf = ctypes.create_string_buffer(raw_sd, len(raw_sd))
    ptr = ctypes.cast(buf, ctypes.c_void_p)
    if not self._ntdll.RtlValidRelativeSecurityDescriptor(ptr, len(raw_sd), 0):
      return None
    return self._sd_ptr_to_sddl(ptr, 0x1F)

  def query_service(self, service_name: str) -> ServiceInfo:
    subkey = rf'SYSTEM\CurrentControlSet\Services\{service_name}'
    if not self.reg_key_exists(subkey, view=64):
      return ServiceInfo(exists=False)
    image_path = str(self.reg_get_value(subkey, 'ImagePath', view=64) or '')
    account = str(self.reg_get_value(subkey, 'ObjectName', view=64) or '')
    start_val = self.reg_get_value(subkey, 'Start', view=64)
    start_type = {2: 'AUTO_START', 3: 'DEMAND_START', 4: 'DISABLED'}.get(
      start_val, str(start_val)
    )
    state = 'UNKNOWN'
    if scm := self._adv.OpenSCManagerW(None, None, 0x0001):
      try:
        if svc := self._adv.OpenServiceW(scm, service_name, 0x0004):
          try:
            st = _ServiceStatus()
            if self._adv.QueryServiceStatus(svc, ctypes.byref(st)):
              state = {
                1: 'STOPPED',
                2: 'START_PENDING',
                3: 'STOP_PENDING',
                4: 'RUNNING',
                5: 'CONTINUE_PENDING',
                6: 'PAUSE_PENDING',
                7: 'PAUSED',
              }.get(st.dwCurrentState, str(st.dwCurrentState))
          finally:
            self._adv.CloseServiceHandle(svc)
      finally:
        self._adv.CloseServiceHandle(scm)
    return ServiceInfo(True, state, start_type, account, image_path)

  def get_service_sddl(self, service_name: str) -> Optional[str]:
    sd = ctypes.c_void_p()
    rc = self._adv.GetNamedSecurityInfoW(
      service_name, 2, 0x7, None, None, None, None, ctypes.byref(sd)
    )
    return self._free_and_format_sd(sd) if rc == 0 else None

  def enum_msi_products(self, upgrade_code: str) -> list[str]:
    products, idx, buf = [], 0, ctypes.create_unicode_buffer(39)
    while self._msi.MsiEnumRelatedProductsW(upgrade_code, 0, idx, buf) == 0:
      products.append(buf.value)
      idx += 1
    return products


def _evaluate_acl(
  entry: InventoryEntry, sddl: Optional[str], **kwargs: Any
) -> None:
  if sddl is None:
    entry.acl_status = 'UNAVAILABLE'
    entry.issues.append('ACL: Security descriptor unavailable')
    return
  entry.acl_sddl = sddl
  if violations := validate_sddl(sddl, **kwargs):
    entry.acl_status = 'INVALID'
    entry.issues.extend(f'ACL: {v}' for v in violations)
  else:
    entry.acl_status = 'OK'
