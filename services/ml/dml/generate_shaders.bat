@ECHO OFF
REM
REM Copyright (c) 2017 The Chromium Project Authors. All rights reserved.
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.
REM

PATH %ProgramFiles(x86)%\Windows Kits\10\bin\10.0.18361.0\x86;%DXSDK_DIR%\Utilities\bin\x86;%PATH%

setlocal
set errorCount=0
set successCount=0
set debug=0

if "%1" == "debug" (
    set debug=1
)
if "%1" == "release" (
    set debug=0
)

::              | Input file             | Entry point  | Type            | Output file           | Debug |
call:BuildShader format_input_shader.hlsl   format_input    cs_5_1  format_input_shader.h    %debug%
call:BuildShader format_output_shader.hlsl  format_output   cs_5_1  format_output_shader.h   %debug%
call:BuildShader half_input_shader.hlsl   format_half_input    cs_5_1  half_input_shader.h    %debug%
call:BuildShader half_output_shader.hlsl  format_half_output   cs_5_1  half_output_shader.h   %debug%

echo.

if %successCount% GTR 0 (
   echo %successCount% shaders compiled successfully.
)
if %errorCount% GTR 0 (
   echo There were %errorCount% shader compilation errors.
)

endlocal
exit /b

:BuildShader
set input=%~1
set entry=%~2
set type=%~3
set output=%~4
set debug=%~5

if %debug% == 0 (
    set "buildCMD=fxc /nologo /E %entry% /T %type% /Fh %output% %input%"
) else (
    set "buildCMD=fxc /nologo /Zi /Od /E %entry% /T %type% /Fh %output% %input%"
)

set error=0
%buildCMD% || set error=1

if %error% == 0 (
    set /a successCount=%successCount%+1
) else (
    set /a errorCount=%errorCount%+1
)

exit /b
