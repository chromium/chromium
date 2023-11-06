:: Copyright 2023 The Abseil Authors
::
:: Licensed under the Apache License, Version 2.0 (the "License");
:: you may not use this file except in compliance with the License.
:: You may obtain a copy of the License at
::
::     https://www.apache.org/licenses/LICENSE-2.0
::
:: Unless required by applicable law or agreed to in writing, software
:: distributed under the License is distributed on an "AS IS" BASIS,
:: WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
:: See the License for the specific language governing permissions and
:: limitations under the License.

SETLOCAL ENABLEDELAYEDEXPANSION

SET ABSL_GOOGLETEST_COMMIT=2dd1c131950043a8ad5ab0d2dda0e0970596586a

IF EXIST %KOKORO_GFILE_DIR%\distdir\%ABSL_GOOGLETEST_COMMIT%.zip (
  SET ABSL_GOOGLETEST_DOWNLOAD_URL=file://%KOKORO_GFILE_DIR%\distdir\%ABSL_GOOGLETEST_COMMIT%.zip
) ELSE (
  SET ABSL_GOOGLETEST_DOWNLOAD_URL=https://github.com/google/googletest/archive/%ABSL_GOOGLETEST_COMMIT%.zip
)

:: Replace '\' with '/' in Windows paths for CMake.
:: Note that this cannot go inside the IF block above, because BAT files are weird.
SET ABSL_GOOGLETEST_DOWNLOAD_URL=%ABSL_GOOGLETEST_DOWNLOAD_URL:\=/%

IF EXIST "C:\Program Files\CMake\bin\" (
  SET CMAKE_BIN="C:\Program Files\CMake\bin\cmake.exe"
  SET CTEST_BIN="C:\Program Files\CMake\bin\ctest.exe"
) ELSE (
  SET CMAKE_BIN="cmake.exe"
  SET CTEST_BIN="ctest.exe"
)

SET CTEST_OUTPUT_ON_FAILURE=1
SET CMAKE_BUILD_PARALLEL_LEVEL=16
SET CTEST_PARALLEL_LEVEL=16

:: Change directory to the root of the project.
CD %~dp0\..
if %errorlevel% neq 0 EXIT /B 1

SET TZDIR=%CD%\absl\time\internal\cctz\testdata\zoneinfo

MKDIR "build"
CD "build"

SET CXXFLAGS="/WX"

%CMAKE_BIN% ^
  -DABSL_BUILD_TEST_HELPERS=ON ^
  -DABSL_BUILD_TESTING=ON ^
  -DABSL_GOOGLETEST_DOWNLOAD_URL=%ABSL_GOOGLETEST_DOWNLOAD_URL% ^
  -DBUILD_SHARED_LIBS=%ABSL_CMAKE_BUILD_SHARED% ^
  -DCMAKE_CXX_STANDARD=%ABSL_CMAKE_CXX_STANDARD% ^
  -G "%ABSL_CMAKE_GENERATOR%" ^
  ..
IF %errorlevel% neq 0 EXIT /B 1

%CMAKE_BIN% --build . --target ALL_BUILD --config %ABSL_CMAKE_BUILD_TYPE%
IF %errorlevel% neq 0 EXIT /B 1

%CTEST_BIN% -C %ABSL_CMAKE_BUILD_TYPE% -E "absl_lifetime_test|absl_symbolize_test"
IF %errorlevel% neq 0 EXIT /B 1

EXIT /B 0
