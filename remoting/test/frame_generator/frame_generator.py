#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Standalone tool for generating synthetic frame sequences using Headless Chrome.
"""

import argparse
import base64
import json
import os
import pathlib
import select
import subprocess
import sys
import time

class DevToolsClient:
    """A simple DevTools Protocol client using pipes."""

    def __init__(self, chrome_path, extra_args):
        # Chrome's FD 3 is read, FD 4 is write on POSIX.
        # On Windows, --remote-debugging-pipe uses different mechanisms,
        # but this tool is primarily targeted at Linux/Mac for now.
        self.p2c_r, self.p2c_w = os.pipe()
        self.c2p_r, self.c2p_w = os.pipe()

        # Set the pipes to be inherited
        os.set_inheritable(self.p2c_r, True)
        os.set_inheritable(self.c2p_w, True)

        args = [
            chrome_path,
            '--headless',
            '--remote-debugging-pipe',
            '--mute-audio',
            '--no-first-run',
            '--disable-gpu',
            '--allow-file-access-from-files',
        ] + extra_args

        # Use a more portable way to pass FDs if possible, but for POSIX,
        # we still need to ensure they land on 3 and 4 for Chrome.
        if os.name == 'posix':
            # We use preexec_fn to dup the FDs to 3 and 4 in the child.
            # While preexec_fn is not portable to Windows, the pipe-based
            # debugging itself requires special handling on Windows anyway.
            def preexec():
                os.dup2(self.p2c_r, 3)
                os.dup2(self.c2p_w, 4)

            self.proc = subprocess.Popen(args,
                                         preexec_fn=preexec,
                                         pass_fds=(self.p2c_r, self.c2p_w))
        else:
            # Fallback for non-POSIX (Windows), though pipe-based CDP
            # might not work out-of-the-box here without further adjustment.
            self.proc = subprocess.Popen(args)

        # Close child's ends in parent
        os.close(self.p2c_r)
        os.close(self.c2p_w)

        self.next_id = 1
        self.responses = {}
        self.events = []
        self.buffer = b''

    def _read_messages(self, timeout=0.1):
        """Reads messages from Chrome and dispatches them."""
        try:
            start_time = time.time()
            while True:
                r, _, _ = select.select([self.c2p_r], [], [], timeout)
                if not r:
                    break

                chunk = os.read(self.c2p_r, 1024 * 1024) # 1MB chunks
                if not chunk:
                    break

                self.buffer += chunk
                while b'\0' in self.buffer:
                    msg_data, self.buffer = self.buffer.split(b'\0', 1)
                    if not msg_data:
                        continue
                    msg = json.loads(msg_data.decode('utf-8'))
                    if 'id' in msg:
                        self.responses[msg['id']] = msg
                    else:
                        self.events.append(msg)

                # If we've been reading for a while, yield
                if time.time() - start_time > 1.0:
                    break
                timeout = 0 # Subsequent reads are non-blocking
        except Exception as e:
            print(f"Error reading from Chrome: {e}", file=sys.stderr)

    def send(self, method, params=None, session_id=None):
        """Sends a command to Chrome."""
        msg_id = self.next_id
        self.next_id += 1
        msg = {'id': msg_id, 'method': method, 'params': params or {}}
        if session_id:
            msg['sessionId'] = session_id
        os.write(self.p2c_w, json.dumps(msg).encode('utf-8') + b'\0')
        return msg_id

    def call(self, method, params=None, session_id=None, timeout=60):
        """Sends a command and waits for the response."""
        msg_id = self.send(method, params, session_id)
        start_time = time.time()
        while msg_id not in self.responses:
            if time.time() - start_time > timeout:
                raise Exception(f"Timeout waiting for response to {method}")
            self._read_messages()
        res = self.responses.pop(msg_id)
        if 'error' in res:
            raise Exception(f"Error in {method}: {res['error']}")
        return res.get('result')

    def wait_for_event(self, method, timeout=30):
        """Waits for a specific event to occur."""
        start_time = time.time()
        while True:
            for i, event in enumerate(self.events):
                if event['method'] == method:
                    return self.events.pop(i)
            if time.time() - start_time > timeout:
                raise Exception(f"Timeout waiting for event {method}")
            self._read_messages()

    def close(self):
        """Closes the connection and terminates Chrome."""
        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
        os.close(self.p2c_w)
        os.close(self.c2p_r)

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    scenarios_dir = os.path.join(script_dir, 'scenarios')

    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--scenario',
        required=True,
        help="The name of the scenario HTML file (without .html extension).")
    parser.add_argument('--width', type=int, default=1920)
    parser.add_argument('--height', type=int, default=1080)
    parser.add_argument('--frames', type=int, default=100)
    parser.add_argument('--fps', type=float, default=30.0)
    parser.add_argument('--out-dir', required=True)
    parser.add_argument('--chrome-path', default='out/Release/chrome')
    args = parser.parse_args()

    if not os.path.exists(args.out_dir):
        os.makedirs(args.out_dir)

    if not os.path.exists(args.chrome_path):
        print(f"Error: Chrome not found at {args.chrome_path}", file=sys.stderr)
        sys.exit(1)

    scenario_file = f'{args.scenario}.html'
    scenario_path = os.path.join(scenarios_dir, scenario_file)
    if not os.path.exists(scenario_path):
        print(f"Error: Scenario file not found at {scenario_path}",
              file=sys.stderr)
        available = [f[:-5] for f in os.listdir(scenarios_dir)
                     if f.endswith('.html')]
        print(f"Available scenarios: {', '.join(sorted(available))}",
              file=sys.stderr)
        sys.exit(1)

    print(f"Generating {args.frames} frames for scenario '{args.scenario}' "
          f"at {args.width}x{args.height} and {args.fps} FPS...",
          file=sys.stderr)

    client = DevToolsClient(args.chrome_path, [
        f'--window-size={args.width},{args.height}',
        '--force-device-scale-factor=1',
    ])

    try:
        # Create a new target and attach to it using the "flat" protocol.
        target = client.call('Target.createTarget', {'url': 'about:blank'})
        if not target or 'targetId' not in target:
            raise Exception("Failed to create target")
        target_id = target['targetId']

        attachment = client.call('Target.attachToTarget', {
            'targetId': target_id,
            'flatten': True
        })
        if not attachment or 'sessionId' not in attachment:
            raise Exception("Failed to attach to target")
        session_id = attachment['sessionId']

        # Enable Page events
        client.call('Page.enable', {}, session_id=session_id)

        # Set viewport size
        client.call('Emulation.setDeviceMetricsOverride', {
            'width': args.width,
            'height': args.height,
            'deviceScaleFactor': 1,
            'mobile': False
        }, session_id=session_id)

        url = pathlib.Path(scenario_path).as_uri()
        client.call('Page.navigate', {'url': url}, session_id=session_id)
        client.wait_for_event('Page.loadEventFired')

        # Generate frames
        for i in range(args.frames):
            t = i / args.fps
            print(f"Generating frame {i+1}/{args.frames} (t={t:.3f})...",
                  end='\r', file=sys.stderr)

            # Advance the scene by calling the benchmark function in the page
            client.call('Runtime.evaluate', {
                'expression': f'window.setBenchmarkTime({t})'
            }, session_id=session_id)

            # Capture screenshot of the current state
            result = client.call('Page.captureScreenshot', {
                'format': 'png'
            }, session_id=session_id)

            if not result or 'data' not in result:
                raise Exception(f"Failed to capture screenshot for frame {i}")

            # Save the PNG data to the output directory
            png_data = base64.b64decode(result['data'])
            filename = os.path.join(args.out_dir, f'frame_{i:04d}.png')
            with open(filename, 'wb') as f:
                f.write(png_data)

        print(f"\nSuccessfully generated {args.frames} frames in "
              f"{args.out_dir}", file=sys.stderr)

    finally:
        client.close()

if __name__ == '__main__':
    main()
