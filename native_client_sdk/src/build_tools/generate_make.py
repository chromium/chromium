# Copyright 2012 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import sys

import buildbot_common
import build_version
import getos
from buildbot_common import ErrorExit
from easy_template import RunTemplateFileIfChanged
from build_paths import SDK_RESOURCE_DIR

def Trace(msg):
  if Trace.verbose:
    sys.stderr.write(str(msg) + '\n')
Trace.verbose = False


def IsExample(desc):
  dest = desc['DEST']
  return dest.startswith(('examples', 'tests', 'getting_started'))


def GenerateSourceCopyList(desc):
  sources = []
  # Some examples use their own Makefile/sources/etc.
  if 'TARGETS' not in desc:
    # Only copy the DATA files.
    return desc.get('DATA', [])

  # Add sources for each target
  for target in desc['TARGETS']:
    sources.extend(target['SOURCES'])

  # And HTML and data files
  sources.extend(desc.get('DATA', []))

  if IsExample(desc):
    sources.append('common.js')
    if not desc.get('NO_PACKAGE_FILES'):
      sources.extend(['icon128.png', 'background.js'])

  return sources


def GetSourcesDict(sources):
  source_map = {}
  for key in ['.c', '.cc']:
    source_list = [fname for fname in sources if fname.endswith(key)]
    if source_list:
      source_map[key] = source_list
    else:
      source_map[key] = []
  return source_map


def GetProjectObjects(source_dict):
  object_list = []
  for key in ['.c', '.cc']:
    for src in source_dict[key]:
      object_list.append(os.path.splitext(src)[0])
  return object_list


def GetPlatforms(plat_list, plat_filter, first_toolchain):
  platforms = []
  for plat in plat_list:
    if plat in plat_filter:
      platforms.append(plat)

  if first_toolchain:
    return [platforms[0]]
  return platforms


def ErrorMsgFunc(text):
  sys.stderr.write(text + '\n')


def AddMakeBat(pepperdir, makepath):
  """Create a simple batch file to execute Make.

  Creates a simple batch file named make.bat for the Windows platform at the
  given path, pointing to the Make executable in the SDK."""

  makepath = os.path.abspath(makepath)
  if not makepath.startswith(pepperdir):
    ErrorExit('Make.bat not relative to Pepper directory: ' + makepath)

  makeexe = os.path.abspath(os.path.join(pepperdir, 'tools'))
  relpath = os.path.relpath(makeexe, makepath)

  fp = open(os.path.join(makepath, 'make.bat'), 'wb')
  outpath = os.path.join(relpath, 'make.exe')

  # Since make.bat is only used by Windows, for Windows path style
  outpath = outpath.replace(os.path.sep, '\\')
  fp.write('@%s %%*\n' % outpath)
  fp.close()


def FindFile(name, srcroot, srcdirs):
  checks = []
  for srcdir in srcdirs:
    srcfile = os.path.join(srcroot, srcdir, name)
    srcfile = os.path.abspath(srcfile)
    if os.path.exists(srcfile):
      return srcfile
    else:
      checks.append(srcfile)

  ErrorMsgFunc('%s not found in:\n\t%s' % (name, '\n\t'.join(checks)))
  return None


def IsNexe(desc):
  for target in desc['TARGETS']:
    if target['TYPE'] == 'main':
      return True
  return False


def ProcessHTML(srcroot, dstroot, desc, toolchains, configs, first_toolchain):
  name = desc['NAME']
  nmf = desc['TARGETS'][0]['NAME']
  outdir = os.path.join(dstroot, desc['DEST'], name)
  srcpath = os.path.join(srcroot, 'index.html')
  dstpath = os.path.join(outdir, 'index.html')

  tools = GetPlatforms(toolchains, desc['TOOLS'], first_toolchain)

  path = "{tc}/{config}"
  replace = {
    'title': desc['TITLE'],
    'attrs':
        'data-name="%s" data-tools="%s" data-configs="%s" data-path="%s"' % (
        nmf, ' '.join(tools), ' '.join(configs), path),
  }
  RunTemplateFileIfChanged(srcpath, dstpath, replace)


def GenerateManifest(srcroot, dstroot, desc):
  outdir = os.path.join(dstroot, desc['DEST'], desc['NAME'])
  srcpath = os.path.join(SDK_RESOURCE_DIR, 'manifest.json.template')
  dstpath = os.path.join(outdir, 'manifest.json')
  permissions = desc.get('PERMISSIONS', [])
  combined_permissions = list(permissions)
  socket_permissions = desc.get('SOCKET_PERMISSIONS', [])
  if socket_permissions:
    combined_permissions.append({'socket': socket_permissions})
  filesystem_permissions = desc.get('FILESYSTEM_PERMISSIONS', [])
  if filesystem_permissions:
    combined_permissions.append({'fileSystem': filesystem_permissions})
  pretty_permissions = json.dumps(combined_permissions,
                                  sort_keys=True, indent=4)
  replace = {
      'name': desc['TITLE'],
      'description': '%s Example' % desc['TITLE'],
      'key': True,
      'channel': None,
      'permissions': pretty_permissions,
      'multi_platform': desc.get('MULTI_PLATFORM', False),
      'version': build_version.ChromeVersionNoTrunk(),
      'min_chrome_version': desc.get('MIN_CHROME_VERSION')
  }
  RunTemplateFileIfChanged(srcpath, dstpath, replace)


def FindAndCopyFiles(src_files, root, search_dirs, dst_dir):
  buildbot_common.MakeDir(dst_dir)
  for src_name in src_files:
    src_file = FindFile(src_name, root, search_dirs)
    if not src_file:
      ErrorExit('Failed to find: ' + src_name)
    dst_file = os.path.join(dst_dir, src_name)
    if os.path.exists(dst_file):
      if os.stat(src_file).st_mtime <= os.stat(dst_file).st_mtime:
        Trace('Skipping "%s", destination "%s" is newer.' % (
            src_file, dst_file))
        continue
    dst_path = os.path.dirname(dst_file)
    if not os.path.exists(dst_path):
      buildbot_common.MakeDir(dst_path)
    buildbot_common.CopyFile(src_file, dst_file)


def ModifyDescInPlace(desc):
  """Perform post-load processing on .dsc file data.

  Currently this consists of:
  - Add -Wall to CXXFLAGS
  """

  for target in desc['TARGETS']:
    target.setdefault('CXXFLAGS', [])
    target['CXXFLAGS'].insert(0, '-Wall')


def ProcessProject(pepperdir, srcroot, dstroot, desc, toolchains, configs=None,
                   first_toolchain=False):
  if not configs:
    configs = ['Debug', 'Release']

  name = desc['NAME']
  out_dir = os.path.join(dstroot, desc['DEST'], name)
  buildbot_common.MakeDir(out_dir)
  srcdirs = desc.get('SEARCH', ['.', SDK_RESOURCE_DIR])

  # Copy sources to example directory
  sources = GenerateSourceCopyList(desc)
  FindAndCopyFiles(sources, srcroot, srcdirs, out_dir)

  # Copy public headers to the include directory.
  for headers_set in desc.get('HEADERS', []):
    headers = headers_set['FILES']
    header_out_dir = os.path.join(dstroot, headers_set['DEST'])
    FindAndCopyFiles(headers, srcroot, srcdirs, header_out_dir)

  make_path = os.path.join(out_dir, 'Makefile')

  outdir = os.path.dirname(os.path.abspath(make_path))
  if getos.GetPlatform() == 'win':
    AddMakeBat(pepperdir, outdir)

  # If this project has no TARGETS, then we don't need to generate anything.
  if 'TARGETS' not in desc:
    return (name, desc['DEST'])

  if IsNexe(desc):
    template = os.path.join(SDK_RESOURCE_DIR, 'Makefile.example.template')
  else:
    template = os.path.join(SDK_RESOURCE_DIR, 'Makefile.library.template')

  # Ensure the order of |tools| is the same as toolchains; that way if
  # first_toolchain is set, it will choose based on the order of |toolchains|.
  tools = [tool for tool in toolchains if tool in desc['TOOLS']]
  if first_toolchain:
    tools = [tools[0]]

  ModifyDescInPlace(desc)

  template_dict = {
    'desc': desc,
    'rel_sdk': '/'.join(['..'] * (len(desc['DEST'].split('/')) + 1)),
    'pre': desc.get('PRE', ''),
    'post': desc.get('POST', ''),
    'tools': tools,
    'sel_ldr': desc.get('SEL_LDR'),
    'targets': desc['TARGETS'],
    'multi_platform': desc.get('MULTI_PLATFORM', False),
  }
  RunTemplateFileIfChanged(template, make_path, template_dict)

  if IsExample(desc):
    ProcessHTML(srcroot, dstroot, desc, toolchains, configs,
                first_toolchain)
    if not desc.get('NO_PACKAGE_FILES'):
      GenerateManifest(srcroot, dstroot, desc)

  return (name, desc['DEST'])


def GenerateMasterMakefile(pepperdir, out_path, targets, deps):
  """Generate a Master Makefile that builds all examples.

  Args:
    pepperdir: NACL_SDK_ROOT
    out_path: Root for output such that out_path+NAME = full path
    targets: List of targets names
  """
  in_path = os.path.join(SDK_RESOURCE_DIR, 'Makefile.index.template')
  out_path = os.path.join(out_path, 'Makefile')
  rel_path = os.path.relpath(pepperdir, os.path.dirname(out_path))
  template_dict = {
    'projects': targets,
    'deps' : deps,
    'rel_sdk' : rel_path,
  }
  RunTemplateFileIfChanged(in_path, out_path, template_dict)
  outdir = os.path.dirname(os.path.abspath(out_path))
  if getos.GetPlatform() == 'win':
    AddMakeBat(pepperdir, outdir)
