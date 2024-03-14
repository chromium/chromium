#!/bin/bash
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

if [ $# -eq 0 ]; then
    echo "Error: Please provide the name of the local test runner arg file"
    exit 1
fi
local_runner_args=$1
source "$local_runner_args"

function create_output_folder() {
  local prefix="run_"
  local random_string=$RANDOM
  local filename="${prefix}${random_string}"
  mkdir "$filename"
  mkdir "${filename}/iossim"
  mkdir "${filename}/output"
  echo "$filename"
}

function validate_required_variables() {
    local variables_list=("$@")  # Get the list of variable names passed
    for var_name in "${variables_list[@]}"; do
        if [ -z "${!var_name}" ]; then
            echo "Error: variable '$var_name' is not set"
            exit 1
        fi
    done
}

common_variables=(
    path
    xcode_path
    mac_toolchain_cmd
    target
)
validate_required_variables "${common_variables[@]}"

# validate Xcode and iOS versions
xcode_plist_file="${xcode_path}/Contents/version.plist"
if [ -f $xcode_plist_file ]; then

    xcode_build_version=$( /usr/libexec/PlistBuddy \
                        -c "Print :ProductBuildVersion" \
                        "$xcode_plist_file" )
    sim_plist_file="${xcode_path}/Contents/Developer/Platforms/ \
                    iPhoneOS.platform/version.plist"
    max_sim_ios_version=$( /usr/libexec/PlistBuddy -c "Print :CFBundleVersion" \
                        "$sim_plist_file" )
    if [ ! -z "$version" ] && \
        [ "$max_sim_ios_version" != "$version" ] && \
        echo -e "$max_sim_ios_version\n$version" | sort -V | head -n 1 | grep -q "^$max_sim_ios_version$"; then

        echo "Error: the max simulator version supported in this Xcode is ${max_sim_ios_version}"
        exit 1
    fi
else
    echo "Xcode does not exist in dir ${xcode_path}. "
    read -p "What Xcode version would you like to install? (e.g. 15c500b): " \
            xcode_build_version
fi

xcode_build_version=$(echo "$xcode_build_version" | tr '[:upper:]' '[:lower:]')

while true; do
    echo "Please choose what kind of tests you would like to run:"
    echo "1. Unit tests on simulator"
    echo "2. EG tests on simulator"
    echo "3. Unit tests on physical device"
    echo "4. EG tests on physical device"
    read -p "Your choice: " choice

    # Input validation
    if [[ $choice =~ ^[1-4]$ ]]; then  # Check if input is a single digit 1-4
        echo "You chose: $choice"
        break # Exit the loop if input is valid
    else
        echo "Invalid input. Please enter a number between 1 and 4."
    fi
done

gtest_filter_arg=""
if [ -n "$gtest_filter" ]; then
    gtest_filter_arg="$gtest_filter_arg--gtest_filter=$gtest_filter"
fi

# Unit tests on simulator
if [ $choice -eq 1 ]; then
    echo "Running unit tests on simulator..."
    test_variables=(
        platform
        version
    )
    validate_required_variables "${test_variables[@]}"

    output_folder=$(create_output_folder)

    ../run.py "$gtest_filter_arg" \
    --app $path/$target \
    --xcode-path $xcode_path \
    --mac-toolchain-cmd $mac_toolchain_cmd \
    --runtime-cache-prefix "${output_folder}/Runtime-ios-" \
    --iossim "${output_folder}/iossim" \
    --platform "$platform" \
    --version $version \
    --out-dir "${output_folder}/output" \
    --xctest \
    --xcode-build-version $xcode_build_version
# EG tests on simulator
elif [ $choice -eq 2 ]; then
    echo "Running EG tests on simulator..."
    test_variables=(
        host_app
        platform
        version
    )
    validate_required_variables "${test_variables[@]}"

    output_folder=$(create_output_folder)

    ../run.py "$gtest_filter_arg" \
    --app $path/$target \
    --host-app $path/$host_app \
    --xcode-path $xcode_path \
    --mac-toolchain-cmd $mac_toolchain_cmd \
    --runtime-cache-prefix "${output_folder}/Runtime-ios-" \
    --iossim "${output_folder}/iossim" \
    --platform "$platform" \
    --version $version \
    --out-dir "${output_folder}/output" \
    --xctest \
    --xcode-build-version $xcode_build_version \
    --xcodebuild-sim-runner
# Unit tests on physical device
# TODO:(b/328282286): physical device testing is not currently working
# due to idevicefs command issues.
elif [ $choice -eq 3 ]; then
    echo "Running unit tests on physical device"

    output_folder=$(create_output_folder)

    ../run.py "$gtest_filter_arg" \
    --app $path/$target \
    --xcode-path $xcode_path \
    --mac-toolchain-cmd $mac_toolchain_cmd \
    --runtime-cache-prefix "${output_folder}/Runtime-ios-" \
    --iossim "${output_folder}/iossim" \
    --out-dir "${output_folder}/output" \
    --xctest \
    --xcode-build-version $xcode_build_version
# EG tests on physical device
# TODO:(b/328282286): physical device testing is not currently working
# due to idevicefs command issues.
elif [ $choice -eq 4 ]; then
    echo "Running EG tests on physical device"
    test_variables=(
        host_app
    )
    validate_required_variables "${test_variables[@]}"

    output_folder=$(create_output_folder)

    ../run.py "$gtest_filter_arg" \
    --app $path/$target \
    --host-app $path/$host_app \
    --xcode-path $xcode_path \
    --mac-toolchain-cmd $mac_toolchain_cmd \
    --runtime-cache-prefix "${output_folder}/Runtime-ios-" \
    --iossim "${output_folder}/iossim" \
    --out-dir "${output_folder}/output" \
    --xctest \
    --xcode-build-version $xcode_build_version \
    --xcodebuild-device-runner
else
    # This should never execute due to the earlier validation
    echo "Unexpected error occurred"
    exit 1
fi

echo "Tests have finished running. Results are saved in $output_folder"

