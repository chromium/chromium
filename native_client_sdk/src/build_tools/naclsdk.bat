@echo off

:: Copyright 2011 The Chromium Authors
:: Use of this source code is governed by a BSD-style license that can be
:: found in the LICENSE file.

setlocal

set SCRIPT_DIR=%~dp0
set SDK_TOOLS=%SCRIPT_DIR%sdk_tools
set SDK_TOOLS_UPDATE=%SCRIPT_DIR%sdk_tools_update
set PYTHON_DIR=%SCRIPT_DIR%python

if exist "%SDK_TOOLS_UPDATE%" (
  echo Updating sdk_tools
  if exist "%SDK_TOOLS%" (
    rmdir /q/s "%SDK_TOOLS%"
  )
  move "%SDK_TOOLS_UPDATE%" "%SDK_TOOLS%"
)

set PYTHONPATH=%SCRIPT_DIR%

python "%SDK_TOOLS%\sdk_update.py" %*
