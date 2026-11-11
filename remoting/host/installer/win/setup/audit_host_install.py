#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Inventories Chrome Remote Desktop Windows host locations and validates ACLs.

Audits the filesystem directories/files, Windows Service, MSI UpgradeCode,
Windows Firewall rules, uninstall/pending-rename locations, and 32-bit/64-bit
registry locations created or modified by the legacy WiX MSI (`chromoting.wxs`)
or `remoting_setup.exe`, reporting presence, target file validity, and security
descriptors/ACLs.
"""

import argparse
from contextlib import contextmanager
import ctypes
from dataclasses import asdict, dataclass, field
import json
import os
import re
import sys
from typing import Any, Iterator, Optional, Sequence
import uuid

if sys.platform == 'win32':
  import winreg

# Keep constants in sync with installer_constants.h, host_payload_files.gni,
# security_descriptor.h, and chromoting.wxs.
SERVICE_NAME = 'chromoting'
EVENT_SOURCE_NAME = 'chromoting'
OMAHA_APP_ID = '{b210701e-ffc4-49e3-932b-370728c72662}'
LEGACY_MSI_UPGRADE_CODE = '{2b21f767-e157-4fa6-963c-55834c1433a6}'
LEGACY_SAWBUCK_PROVIDER_GUID = '{2db51ca1-4fd8-4b88-b5a2-fb8606b66b02}'

RDP_APP_ID = '{52e6fd1a-f16e-49c0-aacb-5436a915448b}'
RDP_RUN_AS = r'NT AUTHORITY\LocalService'
CHROMOTING_TYPELIB_ID = '{b6396c45-b0cc-456b-9f49-f12964ee6df4}'
IRDP_DESKTOP_SESSION_ID = '{6a7699f0-ee43-43e7-aa30-a6738f9bd470}'
# The RdpDesktopSession UUIDv5 salt is deliberately the IRdpDesktopSession IID
# (keep in sync with `rdp_desktop_session_clsid` in //remoting/host/win/).
RDP_DESKTOP_SESSION_SALT_GUID = IRDP_DESKTOP_SESSION_ID.strip('{}')
IRDP_DESKTOP_SESSION_EVENT_HANDLER_ID = '{b59b96da-83cb-40ee-9b91-c377400fc3e3}'
IDAEMON_CONTROL_ID = '{e051a481-6345-4ba1-bdb1-cf7929955268}'
IDAEMON_CONTROL2_ID = '{655bd819-c08c-4b04-80c2-f160739ff6ef}'
UNIVERSAL_MARSHALER_CLSID = '{00020424-0000-0000-c000-000000000046}'

INTERFACE_PROXY_STUBS = (
  (IDAEMON_CONTROL_ID, UNIVERSAL_MARSHALER_CLSID),
  (IDAEMON_CONTROL2_ID, UNIVERSAL_MARSHALER_CLSID),
  (IRDP_DESKTOP_SESSION_ID, IRDP_DESKTOP_SESSION_ID),
  (
    IRDP_DESKTOP_SESSION_EVENT_HANDLER_ID,
    IRDP_DESKTOP_SESSION_EVENT_HANDLER_ID,
  ),
)

HOST_CONFIG_FILENAME = 'host.json'
UNPRIVILEGED_CONFIG_FILENAME = 'host_unprivileged.json'

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

EXPECTED_PAYLOAD_FILES = (
  'remote_assistance_host.exe',
  'remote_assistance_host_uiaccess.exe',
  'remote_open_url.exe',
  'remote_security_key.exe',
  'remote_webauthn.exe',
  'remoting_core.dll',
  'remoting_crashpad_handler.exe',
  'remoting_curtain.exe',
  'remoting_desktop.exe',
  'remoting_host.exe',
  'remoting_native_messaging_host.exe',
  'remoting_network.exe',
  'remoting_peer_connection.exe',
  'remoting_start_host.exe',
  'CREDITS.txt',
  'chromoting.ico',
  'icudtl.dat',
  'com.google.chrome.remote_assistance.json',
  'com.google.chrome.remote_assistance-firefox.json',
  'com.google.chrome.remote_desktop.json',
  'com.google.chrome.remote_desktop-firefox.json',
  'com.google.chrome.remote_webauthn.json',
)

EXPECTED_PAYLOAD_LOWER = frozenset(f.lower() for f in EXPECTED_PAYLOAD_FILES)

_CHROME_NMH = r'SOFTWARE\Google\chrome\NativeMessagingHosts'
_MOZ_NMH = r'SOFTWARE\Mozilla\NativeMessagingHosts'
NATIVE_MESSAGING_HOSTS = (
  rf'{_CHROME_NMH}\com.google.chrome.remote_assistance',
  rf'{_MOZ_NMH}\com.google.chrome.remote_assistance',
  rf'{_CHROME_NMH}\com.google.chrome.remote_desktop',
  rf'{_MOZ_NMH}\com.google.chrome.remote_desktop',
  rf'{_CHROME_NMH}\com.google.chrome.remote_webauthn',
)

VERSION_DIR_RE = re.compile(r'^\d+\.\d+\.\d+\.\d+$')
UNQUOTED_CMD_EXE_RE = re.compile(
  r'^([^"]+?\.(?:exe|dll))(?=\s|$)', re.IGNORECASE
)


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


@dataclass
class _ComViewAudit:
  found: list[str]
  rdp_placeholder: Optional[InventoryEntry]
  ps_roots: dict[str, InventoryEntry]
  ps_entries: dict[str, InventoryEntry]
  iface_entries: dict[str, InventoryEntry]


def compute_rdp_desktop_session_clsid(version: str) -> str:
  """Computes the version-salted UUIDv5 CLSID for RdpDesktopSession."""
  return (
    '{'
    + str(uuid.uuid5(uuid.UUID(RDP_DESKTOP_SESSION_SALT_GUID), version))
    + '}'
  )


def extract_executable_path(command_line: str) -> str:
  """Extracts the executable path from a quoted or unquoted command line."""
  cmd = command_line.strip()
  if not cmd:
    return ''
  if cmd.startswith('"') and (end := cmd.find('"', 1)) != -1:
    return cmd[1:end]
  if match := UNQUOTED_CMD_EXE_RE.match(cmd):
    return match.group(1)
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
  is_container: bool = True,
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

  if require_appcontainer_rx:
    if is_container and not (ac_self_rx and ac_inherit_rx):
      violations.append(
        'Missing AppContainer (AC / S-1-15-2-1) inheritable Read+Execute ACE'
      )
    elif not is_container and not ac_self_rx:
      violations.append(
        'Missing AppContainer (AC / S-1-15-2-1) Read+Execute ACE'
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

  def find_clsids_by_appid(self, app_id: str, view: int = 64) -> list[str]:
    """Returns `SOFTWARE\\Classes\\CLSID` subkeys matching `app_id`."""
    app_low = app_id.lower()
    out: list[str] = []
    for clsid in self.reg_list_subkeys(r'SOFTWARE\Classes\CLSID', view=view):
      val = self.reg_get_value(
        rf'SOFTWARE\Classes\CLSID\{clsid}', 'AppID', view=view
      )
      if isinstance(val, str) and val.lower() == app_low:
        out.append(clsid)
    return sorted(out, key=str.lower)

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

  def find_clsids_by_appid(self, app_id: str, view: int = 64) -> list[str]:
    # `HKLM\SOFTWARE\Classes\CLSID` typically has thousands of subkeys per
    # view; open the root handle once and open each child relative to it.
    app_low = app_id.lower()
    flag = winreg.KEY_WOW64_32KEY if view == 32 else winreg.KEY_WOW64_64KEY
    out: list[str] = []
    try:
      with self._open_hklm(r'SOFTWARE\Classes\CLSID', view) as root:
        idx = 0
        while True:
          try:
            clsid = winreg.EnumKey(root, idx)
          except OSError as exc:
            if getattr(exc, 'winerror', None) == 259:  # ERROR_NO_MORE_ITEMS
              break
            raise
          idx += 1
          try:
            with winreg.OpenKey(
              root, clsid, 0, winreg.KEY_READ | flag
            ) as child:
              val = winreg.QueryValueEx(child, 'AppID')[0]
              if isinstance(val, str) and val.lower() == app_low:
                out.append(clsid)
          except OSError:
            pass
    except FileNotFoundError:
      pass
    return sorted(out, key=str.lower)

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
  """Evaluates `sddl` via `validate_sddl` and updates `entry` ACL fields."""
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


def _add_item(
  report: AuditReport,
  cat: str,
  loc: str,
  src: str,
  present: bool,
  detail: str = '',
  brand: Optional[str] = None,
) -> InventoryEntry:
  """Appends a new `InventoryEntry` to `report` and tracks active branding."""
  if present and brand and brand not in report.detected_branding:
    report.detected_branding.append(brand)
  entry = InventoryEntry(cat, loc, src, present, detail)
  report.entries.append(entry)
  return entry


def _check_target(
  probe: SystemProbe,
  raw: str,
  is_command: bool,
  issues: list[str],
  label: str = '',
) -> None:
  """Validates that a file path or command executable exists on disk."""
  cmd = raw.strip()
  target = extract_executable_path(cmd) if is_command else cmd
  if (
    is_command and not cmd.startswith('"') and any(c.isspace() for c in target)
  ):
    issues.append(f'{label}Unquoted command path with spaces: {raw!r}')
  if not target or not probe.path_exists(target) or probe.is_dir(target):
    issues.append(f'{label}Target file missing or invalid on disk: {raw!r}')


def _audit_reg(
  probe: SystemProbe,
  report: AuditReport,
  subkey: str,
  view: int,
  source: str,
  brand: Optional[str] = None,
  val_name: Optional[str] = None,
  path_value: Optional[str] = None,
  is_command: bool = False,
  **acl_kwargs: Any,
) -> InventoryEntry:
  """Audits an HKLM registry key or value, its SDDL, and any target path."""
  if val_name is not None:
    val = probe.reg_get_value(subkey, val_name, view=view)
    loc = rf'HKLM\{subkey}\{val_name} [{view}-bit]'
    entry = _add_item(
      report, 'Registry', loc, source, val is not None, str(val or ''), brand
    )
  else:
    loc = rf'HKLM\{subkey} [{view}-bit]'
    present = probe.reg_key_exists(subkey, view=view)
    entry = _add_item(report, 'Registry', loc, source, present, brand=brand)
  if entry.present:
    _evaluate_acl(
      entry,
      probe.get_reg_sddl(subkey, view=view),
      object_type='reg',
      **acl_kwargs,
    )
    if path_value is not None:
      raw = str(probe.reg_get_value(subkey, path_value, view=view) or '')
      if val_name is None:
        entry.detail = raw
      _check_target(probe, raw, is_command, entry.issues)
  return entry


def _check_reg_subkey(
  probe: SystemProbe,
  entry: InventoryEntry,
  subkey: str,
  view: int,
  label: str,
  expected_val: Optional[str] = None,
  required: bool = True,
) -> Optional[str]:
  """Validates a registry subkey's presence, default value, and SDDL."""
  if not probe.reg_key_exists(subkey, view=view):
    if required:
      entry.issues.append(f'Missing {label} subkey')
    return None
  val = str(probe.reg_get_value(subkey, '', view=view) or '')
  if expected_val is not None and val.lower() != expected_val.lower():
    entry.issues.append(
      f'Unexpected {label} {val!r} (expected {expected_val!r})'
    )
  if (sub_sddl := probe.get_reg_sddl(subkey, view=view)) is None:
    if entry.acl_status == 'OK':
      entry.acl_status = 'UNAVAILABLE'
    entry.issues.append(f'{label} ACL: Security descriptor unavailable')
  elif sub_viol := validate_sddl(sub_sddl, object_type='reg'):
    entry.acl_status = 'INVALID'
    entry.issues.extend(f'{label} ACL: {v}' for v in sub_viol)
  return val


def _is_expected_com_dir(
  dir_path: str,
  pf64: str,
  pfx86: str,
  specs: Sequence[BrandingSpec],
  allow_64: bool,
  allow_32: bool,
) -> bool:
  """Checks if `dir_path` matches a native 64-bit or versioned 32-bit dir."""
  norm = dir_path.rstrip('\\/').lower()
  if allow_64 and any(
    norm == _join_win_path(pf64, s.rel_dir).lower() for s in specs
  ):
    return True
  if allow_32:
    for spec in specs:
      prefix = _join_win_path(pfx86, spec.rel_dir).lower() + '\\'
      if norm.startswith(prefix) and VERSION_DIR_RE.match(norm[len(prefix) :]):
        return True
  return False


def _check_com_dll_identity(
  entry: InventoryEntry,
  pf64: str,
  pfx86: str,
  specs: Sequence[BrandingSpec],
  allow_64: bool,
  allow_32: bool,
  label: str,
) -> None:
  """Verifies that a COM DLL entry points to `remoting_core.dll` in CRD dir."""
  if not entry.present:
    return
  dll_dir, _, dll_name = entry.detail.strip().rpartition('\\')
  if dll_name.lower() != 'remoting_core.dll' or not _is_expected_com_dir(
    dll_dir, pf64, pfx86, specs, allow_64, allow_32
  ):
    entry.issues.append(f'Unexpected {label} binary: {entry.detail!r}')


def _audit_com_appid(probe: SystemProbe, report: AuditReport) -> InventoryEntry:
  """Audits `HKLM\\SOFTWARE\\Classes\\AppId\\{RDP_APP_ID}` and its COM SDDLs."""
  app_id_subkey = rf'SOFTWARE\Classes\AppId\{RDP_APP_ID}'
  # Human-readable `(Default)` description strings on AppId/CLSID are not used
  # by COM activation and are intentionally not required here.
  app_entry = _audit_reg(probe, report, app_id_subkey, 64, 'both')
  if app_entry.present:
    run_as = str(probe.reg_get_value(app_id_subkey, 'RunAs', view=64) or '')
    app_entry.detail = f'RunAs={run_as!r}'
    if run_as.lower() != RDP_RUN_AS.lower():
      app_entry.issues.append(
        f'Unexpected AppId RunAs {run_as!r} (expected {RDP_RUN_AS!r})'
      )
    for perm in ('AccessPermission', 'LaunchPermission'):
      raw_sd = probe.reg_get_value(app_id_subkey, perm, view=64)
      pentry = _add_item(
        report,
        'Registry',
        rf'HKLM\{app_id_subkey}\{perm} [64-bit]',
        'both',
        raw_sd is not None,
      )
      if raw_sd is None:
        pentry.issues.append(f'Missing {perm} binary value on AppId')
      elif (com_sddl := probe.binary_sd_to_sddl(raw_sd)) is None:
        pentry.acl_status = 'INVALID'
        pentry.issues.append(f'Invalid binary SD in {perm}')
      else:
        pentry.acl_sddl = pentry.detail = com_sddl
        if violations := validate_com_permissions_sddl(com_sddl):
          pentry.acl_status = 'INVALID'
          pentry.issues.extend(f'COM SD: {v}' for v in violations)
        else:
          pentry.acl_status = 'OK'
  return app_entry


def _audit_com_view(
  probe: SystemProbe,
  report: AuditReport,
  view: int,
  src: str,
  salted: Sequence[str],
  pf64: str,
  pfx86: str,
  specs: Sequence[BrandingSpec],
) -> _ComViewAudit:
  """Audits per-view redirected CLSID, ProxyStub, and Interface COM keys."""
  found = [
    c
    for c in salted
    if probe.reg_key_exists(rf'SOFTWARE\Classes\CLSID\{c}', view=view)
  ]
  checked = {c.lower() for c in found}
  enum_error: Optional[str] = None
  try:
    by_appid = probe.find_clsids_by_appid(RDP_APP_ID, view=view)
  except OSError as exc:
    by_appid = []
    enum_error = f'CLSID enumeration failed in {view}-bit view: {exc}'
  for clsid in by_appid:
    if clsid.lower() not in checked:
      checked.add(clsid.lower())
      found.append(clsid)

  rdp_placeholder: Optional[InventoryEntry] = None
  if not found:
    loc = rf'HKLM\SOFTWARE\Classes\CLSID\<RdpDesktopSession> [{view}-bit]'
    rdp_placeholder = _add_item(report, 'Registry', loc, src, False)
    if enum_error is not None:
      rdp_placeholder.issues.append(enum_error)
  for idx, clsid in enumerate(sorted(found, key=str.lower)):
    c_key = rf'SOFTWARE\Classes\CLSID\{clsid}'
    c_root = _audit_reg(probe, report, c_key, view, src)
    if idx == 0 and enum_error is not None:
      c_root.issues.append(enum_error)
    app_val = str(probe.reg_get_value(c_key, 'AppID', view=view) or '')
    c_root.detail = f'AppID={app_val!r}'
    if app_val.lower() != RDP_APP_ID.lower():
      c_root.issues.append(
        f'Mismatched CLSID AppID {app_val!r} (expected {RDP_APP_ID!r})'
      )
    ls_entry = _audit_reg(
      probe,
      report,
      rf'{c_key}\LocalServer32',
      view,
      src,
      path_value='',
      is_command=True,
    )
    if not ls_entry.present:
      c_root.issues.append(f'Missing LocalServer32 subkey under {c_key}')
    else:
      exe = extract_executable_path(ls_entry.detail)
      exe_dir, _, exe_name = exe.rpartition('\\')
      expected_cmd = f'"{exe}" --type=rdp_desktop_session'
      if exe_name.lower() != 'remoting_curtain.exe' or not _is_expected_com_dir(
        exe_dir,
        pf64,
        pfx86,
        specs,
        allow_64=(view == 64),
        allow_32=(view == 32),
      ):
        ls_entry.issues.append(f'Unexpected LocalServer32 binary: {exe!r}')
      elif ls_entry.detail.strip().lower() != expected_cmd.lower():
        ls_entry.issues.append(
          f'Unexpected LocalServer32 command line: {ls_entry.detail!r}'
        )
    _check_reg_subkey(
      probe,
      c_root,
      rf'{c_key}\TypeLib',
      view,
      'CLSID TypeLib',
      CHROMOTING_TYPELIB_ID,
    )
    if idx == 0 and len(found) > 1:
      c_root.issues.append(
        f'Multiple ({len(found)}) RdpDesktopSession CLSIDs in {view}-bit view'
      )

  ps_roots: dict[str, InventoryEntry] = {}
  ps_entries: dict[str, InventoryEntry] = {}
  for ps_id in (IRDP_DESKTOP_SESSION_ID, IRDP_DESKTOP_SESSION_EVENT_HANDLER_ID):
    ps_key = rf'SOFTWARE\Classes\CLSID\{ps_id}'
    ps_roots[ps_id] = _audit_reg(probe, report, ps_key, view, src)
    inproc_key = rf'{ps_key}\InprocServer32'
    pe = _audit_reg(probe, report, inproc_key, view, src, path_value='')
    _check_com_dll_identity(
      pe,
      pf64,
      pfx86,
      specs,
      allow_64=(view == 64),
      allow_32=(view == 32),
      label='InprocServer32',
    )
    # Neither `chromoting.wxs` nor `registry_work_items.cc` writes
    # `ThreadingModel` on `InprocServer32` (or `Version` on
    # `Interface\TypeLib`), so absence is expected; validate `ThreadingModel`
    # equals `'Both'` if present.
    if pe.present:
      tm = probe.reg_get_value(inproc_key, 'ThreadingModel', view=view)
      if tm is not None:
        pe.detail = f'{pe.detail}, ThreadingModel={tm!r}'
        if not isinstance(tm, str) or tm.lower() != 'both':
          pe.issues.append(
            f"Unexpected InprocServer32 ThreadingModel {tm!r} (expected 'Both')"
          )
    ps_entries[ps_id] = pe

  iface_entries: dict[str, InventoryEntry] = {}
  for iid, expected_ps in INTERFACE_PROXY_STUBS:
    iface_subkey = rf'SOFTWARE\Classes\Interface\{iid}'
    ie = _audit_reg(probe, report, iface_subkey, view, src)
    iface_entries[iid] = ie
    if ie.present:
      ps_val = _check_reg_subkey(
        probe,
        ie,
        rf'{iface_subkey}\ProxyStubClsid32',
        view,
        'ProxyStubClsid32',
        expected_ps,
      )
      # Legacy 32-bit `chromoting.wxs` omits `TypeLib` under
      # `Interface\{IRdpDesktopSession}`; `remoting_setup.exe` writes all 4.
      tlb_required = view == 64 or iid != IRDP_DESKTOP_SESSION_ID
      tlb_val = _check_reg_subkey(
        probe,
        ie,
        rf'{iface_subkey}\TypeLib',
        view,
        'Interface TypeLib',
        CHROMOTING_TYPELIB_ID,
        required=tlb_required,
      )
      detail_parts = []
      if ps_val is not None:
        detail_parts.append(f'ProxyStubClsid32={ps_val!r}')
      if tlb_val is not None:
        detail_parts.append(f'TypeLib={tlb_val!r}')
      ie.detail = ', '.join(detail_parts)
  return _ComViewAudit(
    found=found,
    rdp_placeholder=rdp_placeholder,
    ps_roots=ps_roots,
    ps_entries=ps_entries,
    iface_entries=iface_entries,
  )


def _audit_com_typelib(
  probe: SystemProbe,
  report: AuditReport,
  pf64: str,
  pfx86: str,
  specs: Sequence[BrandingSpec],
) -> tuple[InventoryEntry, InventoryEntry, InventoryEntry]:
  """Audits shared `HKLM\\SOFTWARE\\Classes\\TypeLib\\{...}\\1.0` keys."""
  tlb = rf'SOFTWARE\Classes\TypeLib\{CHROMOTING_TYPELIB_ID}\1.0'
  tlb_root = _audit_reg(probe, report, tlb, 64, 'both')
  if tlb_root.present:
    flags_val = _check_reg_subkey(
      probe, tlb_root, rf'{tlb}\FLAGS', 64, 'TypeLib FLAGS', '0'
    )
    helpdir_val = _check_reg_subkey(
      probe, tlb_root, rf'{tlb}\HELPDIR', 64, 'TypeLib HELPDIR'
    )
    if helpdir_val is not None:
      hdir = helpdir_val.strip()
      if not hdir or not probe.path_exists(hdir) or not probe.is_dir(hdir):
        tlb_root.issues.append(
          f'TypeLib HELPDIR missing or not a directory: {helpdir_val!r}'
        )
      elif not _is_expected_com_dir(hdir, pf64, pfx86, specs, True, True):
        tlb_root.issues.append(f'Unexpected TypeLib HELPDIR: {helpdir_val!r}')
    tlb_root.detail = f'FLAGS={flags_val!r}, HELPDIR={helpdir_val!r}'
  tlb_win32 = _audit_reg(
    probe, report, rf'{tlb}\0\win32', 64, 'both', path_value=''
  )
  _check_com_dll_identity(
    tlb_win32, pf64, pfx86, specs, True, True, 'TypeLib win32'
  )
  tlb_win64 = _audit_reg(
    probe, report, rf'{tlb}\0\win64', 64, 'exe', path_value=''
  )
  _check_com_dll_identity(
    tlb_win64, pf64, pfx86, specs, True, False, 'TypeLib win64'
  )
  return tlb_root, tlb_win32, tlb_win64


def _cross_check_com_state(
  app_entry: InventoryEntry,
  com_views: dict[int, _ComViewAudit],
  tlb_root: InventoryEntry,
  tlb_win32: InventoryEntry,
  tlb_win64: InventoryEntry,
) -> None:
  """Flags partial or orphaned COM state across shared and redirected keys.

  Expected valid states (`CLSID` & `Interface` are WOW64-redirected; `AppId` &
  `TypeLib` are shared across views):
  - Clean machine: AppId absent, 64/32 views empty, TypeLib absent.
  - Native 64-bit (`exe`): AppId + 64-bit CLSID/2 PS/4 Iface + TypeLib win32/64.
  - Legacy 32-bit (`msi`): AppId + 32-bit CLSID/2 PS/4 Iface + TypeLib win32.
  """
  any_rdp_clsid = bool(com_views[64].found or com_views[32].found)
  if app_entry.present and not any_rdp_clsid:
    app_entry.issues.append(
      'AppId is present without any registered RdpDesktopSession CLSID'
    )
  elif any_rdp_clsid and not app_entry.present:
    app_entry.issues.append(
      'Missing AppId for registered RdpDesktopSession CLSID'
    )

  for view in (64, 32):
    vstate = com_views[view]
    has_com_server_in_view = (
      bool(vstate.found)
      or any(r.present for r in vstate.ps_roots.values())
      or any(pe.present for pe in vstate.ps_entries.values())
    )
    if has_com_server_in_view:
      if not vstate.found and vstate.rdp_placeholder is not None:
        vstate.rdp_placeholder.issues.append(
          f'Missing RdpDesktopSession CLSID in {view}-bit view '
          '(partial COM registration)'
        )
      for ps_id, pe in vstate.ps_entries.items():
        if not vstate.ps_roots[ps_id].present:
          vstate.ps_roots[ps_id].issues.append(
            f'Missing ProxyStub CLSID in {view}-bit view '
            '(partial COM registration)'
          )
        if not pe.present:
          pe.issues.append(
            f'Missing ProxyStub InprocServer32 in {view}-bit view '
            '(partial COM registration)'
          )
      for ie in vstate.iface_entries.values():
        if not ie.present:
          ie.issues.append(
            f'Missing COM Interface registration in {view}-bit view '
            '(partial COM registration)'
          )
    else:
      for ie in vstate.iface_entries.values():
        if ie.present:
          ie.issues.append(
            f'Orphaned COM Interface registration in {view}-bit view '
            'without COM server'
          )

  if com_views[64].found and not tlb_win64.present:
    tlb_win64.issues.append(
      'Missing TypeLib win64 registration for 64-bit COM server'
    )
  elif tlb_win64.present and not com_views[64].found:
    tlb_win64.issues.append(
      'Orphaned TypeLib win64 registration without 64-bit COM server'
    )
  if any_rdp_clsid and not tlb_win32.present:
    tlb_win32.issues.append('Missing TypeLib win32 registration for COM server')
  elif (
    (tlb_root.present or tlb_win32.present or tlb_win64.present)
    and not any_rdp_clsid
    and not app_entry.present
  ):
    orphan_tlb = (
      tlb_root
      if tlb_root.present
      else (tlb_win64 if tlb_win64.present else tlb_win32)
    )
    orphan_tlb.issues.append('Orphaned TypeLib registration without COM server')


def _audit_com_registrations(
  probe: SystemProbe,
  report: AuditReport,
  versions: Sequence[str],
  pf64: str = r'C:\Program Files',
  pfx86: str = r'C:\Program Files (x86)',
  specs: Sequence[BrandingSpec] = (CHROME_BRANDING, CHROMIUM_BRANDING),
) -> None:
  """Audits COM AppId, CLSIDs, ProxyStubs, Interfaces, TypeLib, and parity."""
  app_entry = _audit_com_appid(probe, report)
  salted = [compute_rdp_desktop_session_clsid(v) for v in versions]
  com_views = {
    view: _audit_com_view(probe, report, view, src, salted, pf64, pfx86, specs)
    for view, src in ((64, 'exe'), (32, 'msi'))
  }
  tlb_root, tlb_win32, tlb_win64 = _audit_com_typelib(
    probe, report, pf64, pfx86, specs
  )
  _cross_check_com_state(app_entry, com_views, tlb_root, tlb_win32, tlb_win64)


def audit_host_install(
  probe: SystemProbe,
  branding_filter: str = 'auto',
  explicit_version: Optional[str] = None,
) -> AuditReport:
  """Inventories CRD MSI/EXE locations and validates their ACLs."""
  report = AuditReport()
  specs = (
    [BRANDING_SPECS[branding_filter]]
    if branding_filter in BRANDING_SPECS
    else [CHROME_BRANDING, CHROMIUM_BRANDING]
  )
  versions: list[str] = [explicit_version] if explicit_version else []

  def check_reparse_ancestors(
    base: str, rel_dir: str, issues: list[str]
  ) -> None:
    cur = base.rstrip('\\/')
    for part in rel_dir.strip('\\/').split('\\'):
      cur = f'{cur}\\{part}'
      if probe.path_exists(cur) and probe.is_reparse_point(cur):
        issues.append(f'Path or ancestor is a reparse point/symlink: {cur}')

  def audit_install_dir(
    base_dir: str, rel_dir: str, source: str, brand: str, check_payload: bool
  ) -> None:
    dir_path = _join_win_path(base_dir, rel_dir)
    entry = _add_item(
      report,
      'Filesystem',
      dir_path,
      source,
      probe.path_exists(dir_path),
      brand=brand,
    )
    if not entry.present:
      return
    check_reparse_ancestors(base_dir, rel_dir, entry.issues)
    if not probe.is_dir(dir_path):
      entry.issues.append('Install path is not a directory')
      _evaluate_acl(entry, probe.get_file_sddl(dir_path), object_type='file')
      return
    _evaluate_acl(
      entry,
      probe.get_file_sddl(dir_path),
      object_type='file',
      require_appcontainer_rx=True,
    )
    items = probe.list_dir(dir_path)
    for name in items:
      if (
        VERSION_DIR_RE.match(name)
        and probe.is_dir(_join_win_path(dir_path, name))
        and name not in versions
      ):
        versions.append(name)
    if not check_payload:
      entry.detail = f'entries: {", ".join(items)}' if items else '(empty)'
      return
    file_items = {
      i.lower() for i in items if not probe.is_dir(_join_win_path(dir_path, i))
    }
    missing = [f for f in EXPECTED_PAYLOAD_FILES if f.lower() not in file_items]
    present_n = len(EXPECTED_PAYLOAD_FILES) - len(missing)
    entry.detail = f'{present_n}/{len(EXPECTED_PAYLOAD_FILES)} payload files'
    if missing:
      entry.issues.append(
        f'Missing {len(missing)} payload file(s): {", ".join(missing)}'
      )
    for name in items:
      full, low = _join_win_path(dir_path, name), name.lower()
      is_sub_dir = probe.is_dir(full)
      is_reparse = probe.is_reparse_point(full)
      is_latest_log_symlink = (
        present_n > 0 and low == 'latest.log' and not is_sub_dir and is_reparse
      )
      is_runtime_artifact = (
        low in ('minidumps', 'crashpad') and is_sub_dir
      ) or (low.endswith('.log') and not is_sub_dir)
      if is_reparse and not is_latest_log_symlink:
        entry.issues.append(f'Unexpected reparse point/symlink: {name}')
      if low.endswith('.old'):
        entry.issues.append(f'Staged .old binary pending cleanup: {name}')
      elif low == 'currentversion':
        if not is_reparse:
          entry.issues.append(f'Unexpected CurrentVersion entry: {name}')
      elif VERSION_DIR_RE.match(name) and is_sub_dir:
        entry.issues.append(f'Legacy MSI versioned directory present: {name}')
      elif is_runtime_artifact:
        if present_n == 0:
          entry.issues.append(f'Leftover runtime artifact: {name}')
      elif is_sub_dir or low not in EXPECTED_PAYLOAD_LOWER:
        entry.issues.append(
          f'Unexpected file/directory in install folder: {name}'
        )
      f_sddl = probe.get_file_sddl(full)
      if f_sddl is None:
        if entry.acl_status == 'OK':
          entry.acl_status = 'UNAVAILABLE'
        entry.issues.append(f'Payload ACL unavailable: {name}')
      elif f_viol := validate_sddl(
        f_sddl,
        object_type='file',
        require_appcontainer_rx=(
          not is_sub_dir and low in EXPECTED_PAYLOAD_LOWER
        ),
        is_container=is_sub_dir,
      ):
        entry.acl_status = 'INVALID'
        entry.issues.extend(f'Payload ACL ({name}): {v}' for v in f_viol)

  pf64 = (
    probe.get_env('ProgramW6432')
    or probe.get_env('ProgramFiles')
    or r'C:\Program Files'
  )
  pfx86 = probe.get_env('ProgramFiles(x86)') or r'C:\Program Files (x86)'
  program_data = probe.get_env('ProgramData') or r'C:\ProgramData'

  # 1. Omaha Client Registration (only registered by Google Chrome-branded
  # official builds; see `BUILDFLAG(GOOGLE_CHROME_BRANDING)` in
  # `registry_work_items.cc` and `OfficialBuild` in `chromoting.wxs`).
  omaha_subkey = rf'SOFTWARE\Google\Update\Clients\{OMAHA_APP_ID}'
  omaha_entries: dict[int, InventoryEntry] = {}
  omaha_pvs: dict[int, str] = {}
  for view, src in ((64, 'exe'), (32, 'both')):
    entry = _audit_reg(
      probe, report, omaha_subkey, view, src, brand=CHROME_BRANDING.name
    )
    omaha_entries[view] = entry
    if entry.present:
      pv = probe.reg_get_value(omaha_subkey, 'pv', view=view)
      name = probe.reg_get_value(omaha_subkey, 'name', view=view)
      entry.detail = f'pv={pv!r}, name={name!r}'
      if not isinstance(pv, str) or not VERSION_DIR_RE.match(pv):
        entry.issues.append(f'Invalid or missing Omaha pv: {pv!r}')
      else:
        omaha_pvs[view] = pv
        if pv not in versions:
          versions.append(pv)

  # 2. Filesystem Install Directories & ProgramData Configuration.
  for spec in specs:
    audit_install_dir(pf64, spec.rel_dir, 'exe', spec.name, True)
    if pfx86.lower() != pf64.lower():
      audit_install_dir(pfx86, spec.rel_dir, 'msi', spec.name, False)

    cfg_dir = _join_win_path(program_data, spec.rel_dir)
    ecfg = _add_item(
      report,
      'ProgramData',
      cfg_dir,
      'both',
      probe.path_exists(cfg_dir),
      brand=spec.name,
    )
    if not ecfg.present:
      continue
    check_reparse_ancestors(program_data, spec.rel_dir, ecfg.issues)
    _evaluate_acl(
      ecfg,
      probe.get_file_sddl(cfg_dir),
      object_type='file',
      require_protected_dacl=True,
      allow_non_admin_read=True,
      allow_trusted_installer=False,
    )
    if not probe.is_dir(cfg_dir):
      ecfg.issues.append('ProgramData config location is not a directory')
      continue
    cfg_items = probe.list_dir(cfg_dir)
    ecfg.detail = f'files: {", ".join(cfg_items)}' if cfg_items else '(empty)'
    for name in cfg_items:
      full_cfg = _join_win_path(cfg_dir, name)
      fentry = _add_item(
        report, 'ProgramData', full_cfg, 'both', True, brand=spec.name
      )
      is_cfg_dir = probe.is_dir(full_cfg)
      if probe.is_reparse_point(full_cfg):
        fentry.issues.append('ProgramData entry is a reparse point/symlink')
      lower = name.lower()
      if not is_cfg_dir and lower.endswith('.json~'):
        fentry.issues.append('Leftover temporary config file (.json~)')
      elif is_cfg_dir or lower not in (
        HOST_CONFIG_FILENAME,
        UNPRIVILEGED_CONFIG_FILENAME,
      ):
        fentry.issues.append('Unexpected file/directory in ProgramData folder')
      _evaluate_acl(
        fentry,
        probe.get_file_sddl(full_cfg),
        object_type='file',
        allow_non_admin_read=(not is_cfg_dir and lower != HOST_CONFIG_FILENAME),
        allow_trusted_installer=False,
      )

  # 3. Windows Service (`chromoting`), MSI UpgradeCode, Uninstall, Firewall.
  svc = probe.query_service(SERVICE_NAME)
  svc_entry = _add_item(
    report, 'Service', rf'SCM\{SERVICE_NAME}', 'both', svc.exists
  )
  if svc.exists:
    svc_exe = extract_executable_path(svc.binary_path)
    svc_entry.detail = (
      f'state={svc.state}, start={svc.start_type}, account={svc.account}, '
      f'binary={svc_exe}'
    )
    _check_target(probe, svc.binary_path, True, svc_entry.issues)
    if svc.account.lower() not in (
      'localsystem',
      r'.\localsystem',
      r'nt authority\system',
    ):
      svc_entry.issues.append(f'Unexpected service account: {svc.account!r}')
    _evaluate_acl(
      svc_entry, probe.get_service_sddl(SERVICE_NAME), object_type='service'
    )
  _audit_reg(
    probe,
    report,
    rf'SYSTEM\CurrentControlSet\Services\{SERVICE_NAME}',
    64,
    'both',
  )

  msi_products = probe.enum_msi_products(LEGACY_MSI_UPGRADE_CODE)
  msi_products_lower = {p.lower() for p in msi_products}
  _add_item(
    report,
    'MSI',
    f'UpgradeCode {LEGACY_MSI_UPGRADE_CODE}',
    'msi',
    bool(msi_products),
    ', '.join(msi_products),
  )

  uninstall_root = r'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall'
  for view in (64, 32):
    for sub in probe.reg_list_subkeys(uninstall_root, view=view):
      sub_path = rf'{uninstall_root}\{sub}'
      sub_low = sub.lower()
      disp = str(probe.reg_get_value(sub_path, 'DisplayName', view=view) or '')
      if (
        sub_low in msi_products_lower
        or 'chrome remote desktop' in disp.lower()
        or 'chromoting' in disp.lower()
      ):
        src = 'msi' if (view == 32 or sub_low in msi_products_lower) else 'both'
        _audit_reg(
          probe, report, sub_path, view, src
        ).detail = f'DisplayName={disp!r}'

  fw_subkey = (
    r'SYSTEM\CurrentControlSet\Services\SharedAccess\Parameters'
    r'\FirewallPolicy\FirewallRules'
  )
  fw_tokens = (
    'remoting_network.exe',
    'remoting_peer_connection.exe',
    'chrome remote desktop',
    'chromoting',
  )
  fw_hits = [
    name
    for name in probe.reg_list_values(fw_subkey, view=64)
    if any(
      t in str(probe.reg_get_value(fw_subkey, name, view=64) or '').lower()
      for t in fw_tokens
    )
  ]
  fw_detail = f'{len(fw_hits)} rule(s): {", ".join(fw_hits)}' if fw_hits else ''
  _add_item(
    report, 'Firewall', rf'HKLM\{fw_subkey}', 'msi', bool(fw_hits), fw_detail
  )

  sm_key = r'SYSTEM\CurrentControlSet\Control\Session Manager'
  pending_ops = probe.reg_get_value(sm_key, 'PendingFileRenameOperations', 64)
  crd_pending = [
    str(op)
    for op in (pending_ops if isinstance(pending_ops, (list, tuple)) else ())
    if any(
      t in str(op).lower()
      for t in ('chrome remote desktop', 'chromoting', 'remoting_')
    )
  ]
  _add_item(
    report,
    'RebootPending',
    rf'HKLM\{sm_key}\PendingFileRenameOperations',
    'both',
    bool(crd_pending),
    '; '.join(crd_pending),
  )

  # 4. Native Messaging, EventLog, and SecurityKey.
  for nm_subkey in NATIVE_MESSAGING_HOSTS:
    for view in (64, 32):
      _audit_reg(probe, report, nm_subkey, view, 'both', path_value='')

  ev_subkey = (
    rf'SYSTEM\CurrentControlSet\Services\EventLog\Application'
    rf'\{EVENT_SOURCE_NAME}'
  )
  ev_entry = _audit_reg(
    probe, report, ev_subkey, 64, 'both', path_value='EventMessageFile'
  )
  if ev_entry.present:
    ev_msg = ev_entry.detail
    cat_msg = str(
      probe.reg_get_value(ev_subkey, 'CategoryMessageFile', view=64) or ''
    )
    ev_entry.detail = (
      f'EventMessageFile={ev_msg!r}, CategoryMessageFile={cat_msg!r}'
    )
    _check_target(
      probe, cat_msg, False, ev_entry.issues, label='CategoryMessageFile: '
    )
  for view, src in ((64, 'exe'), (32, 'msi')):
    _audit_reg(
      probe,
      report,
      r'SOFTWARE\Google\SecurityKey',
      view,
      src,
      val_name='RemoteSecurityKeyPath',
      path_value='RemoteSecurityKeyPath',
    )

  # 5. COM Registrations (AppId + Binary SDs, CLSIDs, Interfaces, TypeLib).
  _audit_com_registrations(probe, report, versions, pf64, pfx86, specs)

  # Cross-check Omaha `pv` against installed filesystem/COM state and 64-bit vs
  # 32-bit view parity so a stale or partial update `pv` is flagged.
  chrome_pf64 = _join_win_path(pf64, CHROME_BRANDING.rel_dir)
  chrome_pf64_has_payload = probe.is_dir(chrome_pf64) and any(
    name.lower() in EXPECTED_PAYLOAD_LOWER
    and not probe.is_dir(_join_win_path(chrome_pf64, name))
    for name in probe.list_dir(chrome_pf64)
  )
  msi_32_pv_match = (
    32 in omaha_pvs
    and probe.is_dir(
      _join_win_path(pfx86, CHROME_BRANDING.rel_dir, omaha_pvs[32])
    )
    and probe.reg_key_exists(
      rf'SOFTWARE\Classes\CLSID'
      rf'\{compute_rdp_desktop_session_clsid(omaha_pvs[32])}',
      view=32,
    )
  )
  for view, pv in omaha_pvs.items():
    salted_pv = compute_rdp_desktop_session_clsid(pv)
    native_match = chrome_pf64_has_payload and probe.reg_key_exists(
      rf'SOFTWARE\Classes\CLSID\{salted_pv}', view=64
    )
    msi_match = view == 32 and msi_32_pv_match
    if not (native_match or msi_match):
      omaha_entries[view].issues.append(
        f'Omaha pv {pv!r} does not match any installed host version'
      )
  if chrome_pf64_has_payload:
    if omaha_entries[64].present and not omaha_entries[32].present:
      omaha_entries[32].issues.append(
        'Missing 32-bit Omaha Client mirror for native Chrome host'
      )
    elif (
      omaha_entries[32].present
      and not omaha_entries[64].present
      and not msi_32_pv_match
    ):
      omaha_entries[64].issues.append(
        'Missing 64-bit Omaha Client registration for native Chrome host'
      )
    elif (
      64 in omaha_pvs
      and 32 in omaha_pvs
      and omaha_pvs[64] != omaha_pvs[32]
      and not msi_32_pv_match
    ):
      omaha_entries[32].issues.append(
        f'Omaha 32-bit pv {omaha_pvs[32]!r} disagrees with '
        f'64-bit pv {omaha_pvs[64]!r}'
      )

  # 6. Branding Keys (URL Forwarder, Pairing/Secrets, Logging) and Sawbuck.
  for spec in specs:
    _audit_reg(
      probe,
      report,
      rf'SOFTWARE\Classes\{spec.url_forwarder_prog_id}\shell\open\command',
      64,
      'both',
      brand=spec.name,
      path_value='',
      is_command=True,
    )
    for view, src in ((64, 'exe'), (32, 'msi')):
      _audit_reg(
        probe,
        report,
        r'SOFTWARE\RegisteredApplications',
        view,
        src,
        brand=spec.name,
        val_name=spec.url_forwarder_name,
      )
      root = rf'SOFTWARE\{spec.rel_dir}'
      for sub in (
        root,
        rf'{root}\UrlForwarder\Capabilities\URLAssociations',
        rf'{root}\logging',
        rf'{root}\paired-clients\clients',
      ):
        _audit_reg(probe, report, sub, view, src, brand=spec.name)
      _audit_reg(
        probe,
        report,
        rf'{root}\paired-clients\secrets',
        view,
        src,
        brand=spec.name,
        require_protected_dacl=True,
        allow_non_admin_read=False,
        allow_trusted_installer=False,
      )

  sawbuck = rf'SOFTWARE\Google\Sawbuck\Providers\{LEGACY_SAWBUCK_PROVIDER_GUID}'
  for view in (32, 64):
    _audit_reg(probe, report, sawbuck, view, 'msi')

  report.present_count = sum(1 for e in report.entries if e.present)
  report.acl_ok_count = sum(1 for e in report.entries if e.acl_status == 'OK')
  report.acl_invalid_count = sum(
    1 for e in report.entries if e.acl_status == 'INVALID'
  )
  report.issue_count = sum(len(e.issues) for e in report.entries)
  return report


def format_report(report: AuditReport, show_absent: bool = False) -> str:
  """Formats an `AuditReport` as a human-readable inventory summary."""
  branding = ', '.join(report.detected_branding) or 'none'
  lines = [
    f'Detected Branding:  {branding}',
    f'Locations Present:  {report.present_count} / {len(report.entries)}',
    f'ACLs Validated OK:  {report.acl_ok_count}',
    f'ACL Violations:     {report.acl_invalid_count}',
    f'Total Issues:       {report.issue_count}',
  ]
  shown = [e for e in report.entries if show_absent or e.present or e.issues]
  if shown:
    lines.append('\nInventory:')
    for e in shown:
      state = 'PRESENT' if e.present else 'ABSENT '
      acl = f' [ACL:{e.acl_status}]' if e.acl_status != 'N/A' else ''
      suffix = f' -> {e.detail}' if e.detail else ''
      lines.append(
        f'  [{state}] ({e.source:4s}) [{e.category}] {e.location}{acl}{suffix}'
      )
      lines.extend(f'      ! {issue}' for issue in e.issues)
  return '\n'.join(lines)


def main(
  argv: Optional[Sequence[str]] = None, probe: Optional[SystemProbe] = None
) -> int:
  parser = argparse.ArgumentParser(
    description='Inventory CRD Windows MSI/EXE locations and validate ACLs.'
  )
  parser.add_argument(
    '--branding',
    choices=('auto', 'chrome', 'chromium'),
    default='auto',
    help='Branding configuration to audit (default: auto).',
  )
  parser.add_argument(
    '--version', help='Optional host version for CLSID lookup.'
  )
  parser.add_argument(
    '--all',
    action='store_true',
    help='Include absent inventory locations in text output.',
  )
  parser.add_argument(
    '--json', action='store_true', help='Output audit report in JSON format.'
  )
  args = parser.parse_args(argv)

  if probe is None:
    if sys.platform != 'win32':
      print('Error: Live probing requires Windows (win32).', file=sys.stderr)
      return 2
    probe = WindowsSystemProbe()

  report = audit_host_install(probe, args.branding, args.version)
  if args.json:
    print(json.dumps(asdict(report), indent=2))
  else:
    print(format_report(report, show_absent=args.all))
  return 0 if report.issue_count == 0 else 1


if __name__ == '__main__':
  sys.exit(main())
