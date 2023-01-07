# Copyright 2013 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

# Create the various paths of interest
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SDK_SRC_DIR = os.path.dirname(SCRIPT_DIR)
SDK_EXAMPLE_DIR = os.path.join(SDK_SRC_DIR, 'examples')
SDK_LIBRARY_DIR = os.path.join(SDK_SRC_DIR, 'libraries')
SDK_RESOURCE_DIR = os.path.join(SDK_SRC_DIR, 'resources')
SDK_DIR = os.path.dirname(SDK_SRC_DIR)
SRC_DIR = os.path.dirname(SDK_DIR)
NACL_DIR = os.path.join(SRC_DIR, 'native_client')
OUT_DIR = os.path.join(SRC_DIR, 'out')
BUILD_ARCHIVE_DIR = os.path.join(OUT_DIR, 'nacl_sdk_build')
EXTRACT_ARCHIVE_DIR = os.path.join(OUT_DIR, 'nacl_sdk_extract')
PPAPI_DIR = os.path.join(SRC_DIR, 'ppapi')
GONACL_APPENGINE_DIR = os.path.join(SDK_SRC_DIR, 'gonacl_appengine')
GONACL_APPENGINE_SRC_DIR = os.path.join(GONACL_APPENGINE_DIR, 'src')

GSTORE = 'https://storage.googleapis.com/nativeclient-mirror/nacl/'
