#!/usr/bin/env vpython3
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
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time


class SimpleWebSocket:
    """A minimal WebSocket client implementation for CDP."""

    def __init__(self, url):
        # url: ws://127.0.0.1:PORT/devtools/browser/GUID
        parts = url[5:].split('/', 1)
        host_port = parts[0].split(':')
        self.host = host_port[0]
        self.port = int(host_port[1])
        self.path = '/' + parts[1]

        self.sock = socket.create_connection((self.host, self.port))
        self._buffer = b""
        self._handshake()

    def _handshake(self):
        key = base64.b64encode(os.urandom(16)).decode()
        handshake = (f"GET {self.path} HTTP/1.1\r\n"
                     f"Host: {self.host}:{self.port}\r\n"
                     f"Upgrade: websocket\r\n"
                     f"Connection: Upgrade\r\n"
                     f"Sec-WebSocket-Key: {key}\r\n"
                     f"Sec-WebSocket-Version: 13\r\n\r\n")
        self.sock.sendall(handshake.encode())

        # Read until end of HTTP headers (\r\n\r\n)
        res = b""
        while b"\r\n\r\n" not in res:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise Exception(
                    "WebSocket handshake failed: connection closed")
            res += chunk

        headers, self._buffer = res.split(b"\r\n\r\n", 1)
        if b"101" not in headers:
            raise Exception(f"WebSocket handshake failed: {headers.decode()}")

    def _recv_all(self, n, deadline=None):
        """Helper to receive exactly n bytes, respecting the deadline."""
        data = b''
        # First consume from the buffer (data read during handshake).
        if self._buffer:
            consume = min(len(self._buffer), n)
            data = self._buffer[:consume]
            self._buffer = self._buffer[consume:]

        while len(data) < n:
            if deadline is not None:
                current_timeout = max(0, deadline - time.time())
                r, _, _ = select.select([self.sock], [], [], current_timeout)
                if not r:
                    raise TimeoutError("Timeout reading from WebSocket")

            chunk = self.sock.recv(n - len(data))
            if not chunk:
                raise RuntimeError("WebSocket connection closed unexpectedly")
            data += chunk
        return data

    def send(self, message):
        data = message.encode()
        length = len(data)
        header = b'\x81'  # Final fragment, text frame
        if length <= 125:
            header += struct.pack('!B', length | 0x80)
        elif length <= 65535:
            header += struct.pack('!B', 126 | 0x80)
            header += struct.pack('!H', length)
        else:
            header += struct.pack('!B', 127 | 0x80)
            header += struct.pack('!Q', length)

        mask = os.urandom(4)
        header += mask
        masked_data = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
        self.sock.sendall(header + masked_data)

    def recv(self, timeout=None):
        """Reads one full message (possibly fragmented) from the socket."""
        full_payload = b""
        deadline = time.time() + timeout if timeout is not None else None
        while True:
            head = self._recv_all(2, deadline=deadline)
            fin = head[0] & 0x80
            opcode = head[0] & 0x0f

            if opcode == 8:  # Close frame
                raise RuntimeError("WebSocket connection closed by server")

            length = head[1] & 0x7f
            if length == 126:
                length = struct.unpack('!H',
                                       self._recv_all(2, deadline=deadline))[0]
            elif length == 127:
                length = struct.unpack('!Q',
                                       self._recv_all(8, deadline=deadline))[0]

            full_payload += self._recv_all(length, deadline=deadline)

            if fin:
                break

        return full_payload.decode('utf-8')

class DevToolsClient:
    """A simple DevTools Protocol client using WebSockets."""

    def __init__(self, chrome_path, extra_args):
        # Use a temporary user data dir to avoid conflicts
        self.user_data_dir = tempfile.mkdtemp(prefix="chromoting_dev_profile_")
        self.ws_url = None

        args = [
            chrome_path,
            '--headless',
            '--remote-debugging-port=0',  # Pick an ephemeral port
            '--mute-audio',
            '--no-first-run',
            '--disable-gpu',
            f'--user-data-dir={self.user_data_dir}',
            '--allow-file-access-from-files',
        ] + extra_args

        self.proc = subprocess.Popen(args,
                                     stderr=subprocess.PIPE,
                                     stdout=subprocess.DEVNULL)

        try:
            # Start a background thread to drain stderr immediately to prevent
            # deadlocks while waiting for the port file.
            self._stop_draining = threading.Event()
            self._drain_thread = threading.Thread(target=self._drain_stderr)
            self._drain_thread.daemon = True
            self._drain_thread.start()

            # Robust discovery of DevTools URL via the user data directory.
            port_file = os.path.join(self.user_data_dir, "DevToolsActivePort")
            start_time = time.time()
            while time.time() - start_time < 30:
                if os.path.exists(port_file):
                    with open(port_file, 'r') as f:
                        lines = f.readlines()
                        if len(lines) >= 2:
                            port = int(lines[0].strip())
                            path = lines[1].strip()
                            self.ws_url = f"ws://127.0.0.1:{port}{path}"
                            break
                if self.proc.poll() is not None:
                    raise Exception("Chrome process exited prematurely")
                time.sleep(0.1)

            if not self.ws_url:
                raise Exception(
                    "Failed to find DevTools port via DevToolsActivePort")

            self.ws = SimpleWebSocket(self.ws_url)
            self.next_id = 1
            self.responses = {}
            self.events = []
        except Exception:
            # Clean up on initialization failure.
            self._stop_draining.set()
            self.proc.kill()
            self.proc.wait()
            shutil.rmtree(self.user_data_dir, ignore_errors=True)
            raise

    def _drain_stderr(self):
        """Continuously reads from stderr to prevent pipe buffer exhaustion."""
        try:
            while not self._stop_draining.is_set():
                if not self.proc.stderr.read1(4096):
                    break
        except Exception:
            # We ignore errors during draining as they usually indicate the
            # process is shutting down.
            pass

    def _read_messages(self, timeout=None):
        """Reads one message from the WebSocket."""
        msg_data = self.ws.recv(timeout=timeout)
        if not msg_data:
            raise RuntimeError("Empty message received from WebSocket")
        msg = json.loads(msg_data)
        if 'id' in msg:
            self.responses[msg['id']] = msg
        else:
            self.events.append(msg)

    def call(self, method, params=None, session_id=None, timeout=60):
        msg_id = self.next_id
        self.next_id += 1
        msg = {'id': msg_id, 'method': method, 'params': params or {}}
        if session_id:
            msg['sessionId'] = session_id

        self.ws.send(json.dumps(msg))

        start_time = time.time()
        while msg_id not in self.responses:
            elapsed = time.time() - start_time
            if elapsed > timeout:
                raise TimeoutError(f"Timeout waiting for response to {method}")
            try:
                self._read_messages(timeout=timeout - elapsed)
            except TimeoutError:
                raise TimeoutError(f"Timeout waiting for response to {method}")

        res = self.responses.pop(msg_id)
        if 'error' in res:
            raise Exception(f"Error in {method}: {res['error']}")
        return res.get('result')

    def wait_for_event(self, method, timeout=30):
        start_time = time.time()
        while True:
            for i, event in enumerate(self.events):
                if event['method'] == method:
                    return self.events.pop(i)

            elapsed = time.time() - start_time
            if elapsed > timeout:
                raise TimeoutError(f"Timeout waiting for event {method}")
            try:
                self._read_messages(timeout=timeout - elapsed)
            except TimeoutError:
                raise TimeoutError(f"Timeout waiting for event {method}")

    def close(self):
        self._stop_draining.set()
        if hasattr(self, 'ws') and self.ws:
            self.ws.sock.close()

        self.proc.terminate()
        try:
            self.proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait()

        shutil.rmtree(self.user_data_dir, ignore_errors=True)

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

    chrome_path = args.chrome_path
    if os.path.isdir(chrome_path):
        for candidate in ['chrome', 'chromium', 'google-chrome', 'chrome.exe']:
            candidate_path = os.path.join(chrome_path, candidate)
            if (os.path.exists(candidate_path)
                    and not os.path.isdir(candidate_path)):
                chrome_path = candidate_path
                break
    elif not os.path.exists(chrome_path):
        path_binary = shutil.which(chrome_path)
        if path_binary:
            chrome_path = path_binary

    if not os.path.exists(chrome_path) or os.path.isdir(chrome_path):
        print(f"Error: Chrome binary not found at {chrome_path}",
              file=sys.stderr)
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

    client = DevToolsClient(chrome_path, [
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
