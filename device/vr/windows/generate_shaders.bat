@ECHO OFF
REM
REM Copyright 2017 The Chromium Project Authors
REM Use of this source code is governed by a BSD-style license that can be
REM found in the LICENSE file.
REM

PATH %ProgramFiles(x86)%\Windows Kits\8.1\bin\x86;%DXSDK_DIR%\Utilities\bin\x86;%PATH%

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
call:BuildShader vertex_shader.hlsl vertex    vs_4_0_level_9_3  vertex_shader.h    %debug%
call:BuildShader flip_pixel_shader.hlsl  flip_pixel     ps_4_0_level_9_3  flip_pixel_shader.h     %debug%
call:BuildShader geometry_shader.hlsl  geometry     gs_5_0  geometry_shader.h     %debug%

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
