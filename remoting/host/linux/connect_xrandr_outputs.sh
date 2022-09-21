#!/bin/bash
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# On GNOME we have observed that when a new mode is added to a disconnected
# randr output, all existing outputs will be resized to 1024x768. This script
# helps connect xrandr outputs before the host is being used, so that existing
# displays won't get resized when the user adds displays in a remote session.

WIDTH=1600
HEIGHT=1200
MODE="${WIDTH}x${HEIGHT}"

# Create the mode in case it hasn't been created yet.
xrandr --newmode "$MODE" 60 "$WIDTH" 0 0 0 "$HEIGHT" 0 0 0

# The client supports adding up to 3 displays (plus the existing DUMMY0) so we
# connect DUMMY 1-3.
connected_outputs=()
for n in $(seq 1 3); do
  query_result="$(xrandr -q)"
  n_1=$((n - 1))
  current_output="DUMMY$n"
  previous_output="DUMMY$n_1"

  if ! echo "$query_result" | grep -q "$previous_output connected"; then
    >&2 echo "Unexpected state: $previous_output is not connected"
    exit 1
  fi

  if echo "$query_result" | grep -q "$current_output connected"; then
    >&2 echo "$current_output is already connected; no change will be made"
    continue
  fi

  xrandr --addmode "$current_output" "$MODE"
  xrandr --output "$current_output" --mode $MODE --right-of "$previous_output"
  connected_outputs+=("$current_output")
  echo "$current_output connected"
done

# If we turn off displays immediately, GNOME will turn them back on, so we need
# to wait here.
echo "Waiting for 2 seconds..."
sleep 2

# Turn off outputs in reverse order.
for (( idx=${#connected_outputs[@]}-1 ; idx>=0 ; idx-- )) ; do
  current_output="${connected_outputs[idx]}"
  xrandr --output "$current_output" --off
  echo "$current_output turned off"
done
