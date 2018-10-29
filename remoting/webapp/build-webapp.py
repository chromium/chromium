#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Creates a directory with with the unpacked contents of the remoting webapp.

The directory will contain a copy-of or a link-to to all remoting webapp
resources.  This includes HTML/JS and any plugin binaries. The script also
massages resulting files appropriately with host plugin data. Finally,
a zip archive for all of the above is produced.
"""

# Python 2.5 compatibility
from __future__ import with_statement

import argparse
import io
import os
import platform
import re
import shutil
import subprocess
import sys
import time
import zipfile

sys.path.append(os.path.join(
    os.path.dirname(__file__), os.pardir, os.pardir,
    "build", "android", "gyp"))
from util import build_utils

# Update the module path, assuming that this script is in src/remoting/webapp,
# and that the google_api_keys module is in src/google_apis. Note that
# sys.path[0] refers to the directory containing this script.
if __name__ == '__main__':
  sys.path.append(
      os.path.abspath(os.path.join(sys.path[0], '../../google_apis')))
import google_api_keys


def findAndReplace(filepath, findString, replaceString):
  """Does a search and replace on the contents of a file."""
  oldFilename = os.path.basename(filepath) + '.old'
  oldFilepath = os.path.join(os.path.dirname(filepath), oldFilename)
  shutil.move(filepath, oldFilepath)
  with open(oldFilepath) as input:
    with open(filepath, 'w') as output:
      for s in input:
        output.write(s.replace(findString, replaceString))
  os.remove(oldFilepath)


def replaceString(destination, placeholder, value):
  findAndReplace(os.path.join(destination, 'plugin_settings.js'),
                 "'" + placeholder + "'", "'" + value + "'")


def replaceBool(destination, placeholder, value):
  # Look for a "!!" in the source code so the expession we're
  # replacing looks like a boolean to the compiler.  A single "!"
  # would satisfy the compiler but might confused human readers.
  findAndReplace(os.path.join(destination, 'plugin_settings.js'),
                 "!!'" + placeholder + "'", 'true' if value else 'false')


def parseBool(boolStr):
  """Tries to parse a string as a boolean value.

  Returns a bool on success; raises ValueError on failure.
  """
  lower = boolStr.lower()
  if lower in ['0', 'false']: return False
  if lower in ['1', 'true']: return True
  raise ValueError('not a boolean string {!r}'.format(boolStr))


def getenvBool(name, defaultValue):
  """Gets an environment value as a boolean."""
  rawValue = os.environ.get(name)
  if rawValue is None:
    return defaultValue
  try:
    return parseBool(rawValue)
  except ValueError:
    raise Exception('Value of ${} must be boolean!'.format(name))


def processJinjaTemplate(input_file, include_paths, output_file, context):
  jinja2_path = os.path.normpath(
      os.path.join(os.path.abspath(__file__),
                   '../../../third_party/jinja2'))
  sys.path.append(os.path.split(jinja2_path)[0])
  import jinja2
  (template_path, template_name) = os.path.split(input_file)
  include_paths = [template_path] + include_paths
  env = jinja2.Environment(loader=jinja2.FileSystemLoader(include_paths))
  template = env.get_template(template_name)
  rendered = template.render(context)
  io.open(output_file, 'w', encoding='utf-8').write(rendered)


def buildWebApp(buildtype, version, destination, zip_path,
                manifest_template, appid, app_client_id, app_name,
                app_description, app_capabilities, manifest_key, files,
                files_listfile, locales_listfile, jinja_paths,
                service_environment, use_gcd):
  """Does the main work of building the webapp directory and zipfile.

  Args:
    buildtype: the type of build ("Official", "Release" or "Dev").
    destination: A string with path to directory where the webapp will be
                 written.
    zipfile: A string with path to the zipfile to create containing the
             contents of |destination|.
    manifest_template: jinja2 template file for manifest.
    appid: A string with the Remoting Application Id (only used for app
           remoting webapps). If supplied, it defaults to using the
           test API server.
    app_client_id: The OAuth2 client ID for the webapp.
    app_name: A string with the name of the application.
    app_description: A string with the description of the application.
    app_capabilities: A set of strings naming the capabilities that should be
                      enabled for this application.
    manifest_key: The manifest key for the webapp.
    files: An array of strings listing the paths for resources to include
           in this webapp.
    files_listfile: The name of a file containing a list of files, one per
                    line, identifying the resources to include in this webapp.
                    This is an alternate to specifying the files directly via
                    the 'files' option. The files listed in this file are
                    appended to the files passed via the 'files' option, if any.
    locales_listfile: The name of a file containing a list of locales, one per
                      line, which are copied, along with their directory
                      structure, from the _locales directory down.
    jinja_paths: An array of paths to search for {%include} directives in
                 addition to the directory containing the manifest template.
    service_environment: Used to point the webapp to the dev/prod environments.
    use_gcd: True if GCD support should be enabled.
  """

  # Load the locales files from the locales_listfile.
  if not locales_listfile:
    raise Exception('You must specify a locales_listfile')
  locales = []
  with open(locales_listfile) as input:
    for s in input:
      locales.append(s.rstrip())

  # Load the files from the files_listfile.
  if files_listfile:
    with open(files_listfile) as input:
      for s in input:
        files.append(s.rstrip())

#Ensure a fresh directory.
  try:
    shutil.rmtree(destination)
  except OSError:
    if os.path.exists(destination):
      raise
    else:
      pass
  os.makedirs(destination, 0775)

  if buildtype != 'Official' and buildtype != 'Release' and buildtype != 'Dev':
    raise Exception('Unknown buildtype: ' + buildtype)

  jinja_context = {
    'buildtype': buildtype,
  }

  # Copy all the files.
  for current_file in files:
    destination_file = os.path.join(destination, os.path.basename(current_file))

    # Process *.jinja2 files as jinja2 templates
    if current_file.endswith(".jinja2"):
      destination_file = destination_file[:-len(".jinja2")]
      processJinjaTemplate(current_file, jinja_paths,
                           destination_file, jinja_context)
    else:
      shutil.copy2(current_file, destination_file)

  # Copy all the locales, preserving directory structure
  destination_locales = os.path.join(destination, '_locales')
  os.mkdir(destination_locales, 0775)
  remoting_locales = os.path.join(destination, 'remoting_locales')
  os.mkdir(remoting_locales, 0775)
  for current_locale in locales:
    extension = os.path.splitext(current_locale)[1]
    if extension == '.json':
      locale_id = os.path.split(os.path.split(current_locale)[0])[1]
      destination_dir = os.path.join(destination_locales, locale_id)
      destination_file = os.path.join(destination_dir,
                                      os.path.split(current_locale)[1])
      os.mkdir(destination_dir, 0775)
      shutil.copy2(current_locale, destination_file)
    elif extension == '.pak':
      destination_file = os.path.join(remoting_locales,
                                      os.path.split(current_locale)[1])
      shutil.copy2(current_locale, destination_file)
    else:
      raise Exception('Unknown extension: ' + current_locale)

  is_prod_service_environment = service_environment == 'prod'

  # Allow host names for google services/apis to be overriden via env vars.
  oauth2AccountsHost = os.environ.get(
      'OAUTH2_ACCOUNTS_HOST', 'https://accounts.google.com')
  oauth2ApiHost = os.environ.get(
      'OAUTH2_API_HOST', 'https://www.googleapis.com')
  directoryApiHost = os.environ.get(
      'DIRECTORY_API_HOST', 'https://www.googleapis.com')
  remotingApiHost = os.environ.get(
      'REMOTING_API_HOST', 'https://remoting-pa.googleapis.com')

  oauth2BaseUrl = oauth2AccountsHost + '/o/oauth2'
  oauth2ApiBaseUrl = oauth2ApiHost + '/oauth2'
  directoryApiBaseUrl = directoryApiHost + '/chromoting/v1'
  telemetryApiBaseUrl = remotingApiHost + '/v1/events'

  replaceBool(destination, 'USE_GCD', use_gcd)
  replaceString(destination, 'OAUTH2_BASE_URL', oauth2BaseUrl)
  replaceString(destination, 'OAUTH2_API_BASE_URL', oauth2ApiBaseUrl)
  replaceString(destination, 'DIRECTORY_API_BASE_URL', directoryApiBaseUrl)
  replaceString(destination, 'TELEMETRY_API_BASE_URL', telemetryApiBaseUrl)

  # Substitute hosts in the manifest's CSP list.
  # Ensure we list the API host only once if it's the same for multiple APIs.
  googleApiHosts = ' '.join(set([oauth2ApiHost, directoryApiHost]))

  # WCS and the OAuth trampoline are both hosted on talkgadget. Split them into
  # separate suffix/prefix variables to allow for wildcards in manifest.json.
  talkGadgetHostSuffix = os.environ.get(
      'TALK_GADGET_HOST_SUFFIX', 'talkgadget.google.com')
  talkGadgetHostPrefix = os.environ.get(
      'TALK_GADGET_HOST_PREFIX', 'https://chromoting-client.')
  oauth2RedirectHostPrefix = os.environ.get(
      'OAUTH2_REDIRECT_HOST_PREFIX', 'https://chromoting-oauth.')

  # Use a wildcard in the manifest.json host specs if the prefixes differ.
  talkGadgetHostJs = talkGadgetHostPrefix + talkGadgetHostSuffix
  talkGadgetBaseUrl = talkGadgetHostJs + '/talkgadget'
  if talkGadgetHostPrefix == oauth2RedirectHostPrefix:
    talkGadgetHostJson = talkGadgetHostJs
  else:
    talkGadgetHostJson = 'https://*.' + talkGadgetHostSuffix

  # Set the correct OAuth2 redirect URL.
  oauth2RedirectHostJs = oauth2RedirectHostPrefix + talkGadgetHostSuffix
  oauth2RedirectHostJson = talkGadgetHostJson
  oauth2RedirectPath = '/talkgadget/oauth/chrome-remote-desktop'
  oauth2RedirectBaseUrlJs = oauth2RedirectHostJs + oauth2RedirectPath
  oauth2RedirectBaseUrlJson = oauth2RedirectHostJson + oauth2RedirectPath
  if buildtype == 'Official':
    oauth2RedirectUrlJs = ("'" + oauth2RedirectBaseUrlJs +
                           "/rel/' + chrome.i18n.getMessage('@@extension_id')")
    oauth2RedirectUrlJson = oauth2RedirectBaseUrlJson + '/rel/*'
  else:
    oauth2RedirectUrlJs = "'" + oauth2RedirectBaseUrlJs + "/dev'"
    oauth2RedirectUrlJson = oauth2RedirectBaseUrlJson + '/dev*'
  thirdPartyAuthUrlJs = oauth2RedirectBaseUrlJs + '/thirdpartyauth'
  thirdPartyAuthUrlJson = oauth2RedirectBaseUrlJson + '/thirdpartyauth*'
  xmppServer = os.environ.get('XMPP_SERVER', 'talk.google.com:443')

  replaceString(destination, 'TALK_GADGET_URL', talkGadgetBaseUrl)
  findAndReplace(os.path.join(destination, 'plugin_settings.js'),
                 "'OAUTH2_REDIRECT_URL'", oauth2RedirectUrlJs)

  # Configure xmpp server and directory bot settings in the plugin.
  xmpp_server_user_tls = getenvBool('XMPP_SERVER_USE_TLS', True)
  if (buildtype != 'Dev' and not xmpp_server_user_tls):
    raise Exception('TLS can must be enabled in non Dev builds.')

  replaceBool(
      destination, 'XMPP_SERVER_USE_TLS', xmpp_server_user_tls)
  replaceString(destination, 'XMPP_SERVER', xmppServer)
  replaceString(destination, 'DIRECTORY_BOT_JID',
                os.environ.get('DIRECTORY_BOT_JID',
                               'remoting@bot.talk.google.com'))
  replaceString(destination, 'THIRD_PARTY_AUTH_REDIRECT_URL',
                thirdPartyAuthUrlJs)

  # Set the correct API keys.
  # For overriding the client ID/secret via env vars, see google_api_keys.py.
  apiClientId = google_api_keys.GetClientID('REMOTING')
  apiClientSecret = google_api_keys.GetClientSecret('REMOTING')

  apiClientIdV2 = os.environ.get(
      'REMOTING_IDENTITY_API_CLIENT_ID',
      google_api_keys.GetClientID('REMOTING_IDENTITY_API'))

  replaceString(destination, 'API_CLIENT_ID', apiClientId)
  replaceString(destination, 'API_CLIENT_SECRET', apiClientSecret)

  # Use a fixed key in the app manifest. For dev builds, this ensures that the
  # app can be run directly from the output directory. For official CRD builds,
  # it allows QA to test the app without uploading it to Chrome Web Store.
  manifest_key = 'remotingdevbuild'

  # Generate manifest.
  if manifest_template:
    context = {
        'FULL_APP_VERSION': version,
        'MANIFEST_KEY': manifest_key,
        'OAUTH2_REDIRECT_URL': oauth2RedirectUrlJson,
        'TALK_GADGET_HOST': talkGadgetHostJson,
        'THIRD_PARTY_AUTH_REDIRECT_URL': thirdPartyAuthUrlJson,
        'REMOTING_IDENTITY_API_CLIENT_ID': apiClientIdV2,
        'OAUTH2_BASE_URL': oauth2BaseUrl,
        'OAUTH2_API_BASE_URL': oauth2ApiBaseUrl,
        'DIRECTORY_API_BASE_URL': directoryApiBaseUrl,
        'TELEMETRY_API_BASE_URL':telemetryApiBaseUrl ,
        'CLOUD_PRINT_URL': '',
        'OAUTH2_ACCOUNTS_HOST': oauth2AccountsHost,
        'GOOGLE_API_HOSTS': googleApiHosts,
        'APP_NAME': app_name,
        'APP_DESCRIPTION': app_description,
        'OAUTH_CLOUD_PRINT_SCOPE': '',
        'OAUTH_GDRIVE_SCOPE': '',
        'USE_GCD': use_gcd,
        'XMPP_SERVER': xmppServer,
        # An URL match pattern that is added to the |permissions| section of the
        # manifest in case some URLs are redirected by corporate proxies.
        'PROXY_URL' : os.environ.get('PROXY_URL', ''),
    }
    if 'CLOUD_PRINT' in app_capabilities:
      context['OAUTH_CLOUD_PRINT_SCOPE'] = ('"https://www.googleapis.com/auth/cloudprint",')
      context['CLOUD_PRINT_URL'] = ('"https://www.google.com/cloudprint/*",')
    if 'GOOGLE_DRIVE' in app_capabilities:
      context['OAUTH_GDRIVE_SCOPE'] = ('"https://docs.google.com/feeds/", '
                                       '"https://www.googleapis.com/auth/drive",')
    processJinjaTemplate(manifest_template,
                         jinja_paths,
                         os.path.join(destination, 'manifest.json'),
                         context)

  # Make the zipfile.
  build_utils.ZipDir(
    zip_path, destination,
    compress_fn=lambda _: zipfile.ZIP_DEFLATED,
    zip_prefix_path=os.path.splitext(os.path.basename(zip_path))[0])

  return 0


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('buildtype')
  parser.add_argument('version')
  parser.add_argument('destination')
  parser.add_argument('zip_path')
  parser.add_argument('manifest_template')
  parser.add_argument('files', nargs='*', metavar='file', default=[])
  parser.add_argument('--app_name', metavar='NAME')
  parser.add_argument('--app_description', metavar='TEXT')
  parser.add_argument('--app_capabilities',
                      nargs='*', default=[], metavar='CAPABILITY')
  parser.add_argument('--appid')
  parser.add_argument('--app_client_id', default='')
  parser.add_argument('--manifest_key', default='')
  parser.add_argument('--files_listfile', default='', metavar='PATH')
  parser.add_argument('--locales_listfile', default='', metavar='PATH')
  parser.add_argument('--jinja_paths', nargs='*', default=[], metavar='PATH')
  parser.add_argument('--service_environment', default='', metavar='ENV')
  parser.add_argument('--use_gcd', choices=['0', '1'], default='0')

  args = parser.parse_args()
  args.use_gcd = (args.use_gcd != '0')
  args.app_capabilities = set(args.app_capabilities)
  return buildWebApp(**vars(args))


if __name__ == '__main__':
  sys.exit(main())
