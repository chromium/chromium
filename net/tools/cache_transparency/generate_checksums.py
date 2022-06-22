# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Generates checksums for URLs in the Pervasive Payload list."""

import argparse
import csv
import requests
import hashlib
import urllib.parse

INCLUDE_HEADERS = frozenset([
    "access-control-allow-credentials", "access-control-allow-headers",
    "access-control-allow-methods", "access-control-allow-origin",
    "access-control-expose-headers", "access-control-max-age",
    "access-control-request-headers", "access-control-request-method",
    "clear-site-data", "content-encoding", "content-security-policy",
    "content-type", "cross-origin-embedder-policy",
    "cross-origin-opener-policy", "cross-origin-resource-policy", "location",
    "sec-websocket-accept", "sec-websocket-extensions", "sec-websocket-key",
    "sec-websocket-protocol", "sec-websocket-version", "upgrade", "vary"
])


def generate_list_with_checksums(data):
  pairs_list = []
  flat_list = []
  for i, url_info in enumerate(data):
    checksum_input = ""
    url = url_info[0]
    print(f"[{i}/{len(data)}] Fetching {url}")

    with requests.get(url,
                      headers={"Accept-Encoding": "gzip, deflate, br"},
                      stream=True) as response:

      headers = list(response.headers.items())
      headers = [(name.lower(), value) for name, value in headers]
      headers.sort()

      for header in headers:
        if header[0] in INCLUDE_HEADERS:
          checksum_input += header[0] + ": " + header[1] + "\n"

      checksum_input += "\n"
      checksum_input = checksum_input.encode()

      raw_body = response.raw.data
      checksum_input += raw_body

    checksum = hashlib.sha256(checksum_input).hexdigest().upper()
    pairs_list.append([url, checksum])
    flat_list.append(str(url))
    flat_list.append(str(checksum))

  return pairs_list, flat_list


def main():
  parser = argparse.ArgumentParser(
      description=__doc__, formatter_class=argparse.RawTextHelpFormatter)

  parser.add_argument(
      "input",
      type=str,
      nargs=1,
      help="path for input csv file containing pervasive payloads list")

  parser.add_argument("-v",
                      "--list-version",
                      "--version",
                      dest="list_version",
                      default="1",
                      help="version of pervasive payloads list")

  parser.add_argument("-f",
                      "--format",
                      dest="format",
                      default="csv",
                      choices=["csv", "comma_separated", "url_encoded"],
                      help="output format to use. Default: csv")

  parser.add_argument("output",
                      type=str,
                      nargs=1,
                      help="path for output file for URLs and checksums")

  args = parser.parse_args()

  filename = args.input[0]

  data = []
  with open(filename, mode="r", newline="") as csvfile:
    datareader = csv.reader(csvfile)
    data = list(datareader)

  pairs_list, flat_list = generate_list_with_checksums(data)

  if args.format == "csv":
    with open(args.output[0], mode="w", newline="") as f:
      writer = csv.writer(f)
      writer.writerows(pairs_list)

  elif args.format == "comma_separated":
    flat_list.insert(0, str(args.list_version))
    with open(args.output[0], mode="w") as file:
      file.write(",\n".join(flat_list))

  elif args.format == "url_encoded":
    concatenated = str(args.list_version) + ","
    concatenated += ",".join(flat_list)
    url_encoded_list = urllib.parse.quote_plus(concatenated)
    with open(args.output[0], mode="w") as file:
      file.write(url_encoded_list)
    print(
        "NOTE: To run the feature via commandline, use the following command:\n"
        "out/Default/chrome --enable-features='PervasivePayloadsList:pervasive-payloads/(url_encoded_list),CacheTransparency,SplitCacheByNetworkIsolationKey'"
    )


if __name__ == "__main__":
  main()
