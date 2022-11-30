#!/usr/bin/env python
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Transform CBCM Takeout API Data (Python2)."""

from __future__ import print_function

import argparse
import csv
import json
import sys

import google_auth_httplib2

from httplib2 import Http
from google.oauth2.service_account import Credentials


def ComputeExtensionsList(extensions_list, data):
  """Computes list of machines that have an extension.

  This sample function processes the |data| retrieved from the Takeout API and
  calculates the list of machines that have installed each extension listed in
  the data.

  Args:
    extensions_list: the extension list dictionary to fill.
    data: the data fetched from the Takeout API.
  """
  for device in data['browsers']:
    if 'browsers' not in device:
      continue
    for browser in device['browsers']:
      if 'profiles' not in browser:
        continue
      for profile in browser['profiles']:
        if 'extensions' not in profile:
          continue
        for extension in profile['extensions']:
          key = extension['extensionId']
          if 'version' in extension:
            key = key + ' @ ' + extension['version']
          if key not in extensions_list:
            current_extension = {
                'name': extension.get('name', ''),
                'permissions': extension.get('permissions', ''),
                'installed': set(),
                'disabled': set(),
                'forced': set()
            }
          else:
            current_extension = extensions_list[key]

          machine_name = device['machineName']
          current_extension['installed'].add(machine_name)
          if extension.get('installType', '') == 'ADMIN':
            current_extension['forced'].add(machine_name)
          if extension.get('disabled', False):
            current_extension['disabled'].add(machine_name)

          extensions_list[key] = current_extension


def ToUtf8(data):
  """Ensures all the values in |data| are encoded as UTF-8.

  Expects |data| to be a list of dict objects.

  Args:
    data: the data to be converted to UTF-8.

  Yields:
    A list of dict objects whose values have been encoded as UTF-8.
  """
  for entry in data:
    for prop, value in entry.iteritems():
      entry[prop] = unicode(value).encode('utf-8')
    yield entry


def DictToList(data, key_name='id'):
  """Converts a dict into a list.

  The value of each member of |data| must also be a dict. The original key for
  the value will be inlined into the value, under the |key_name| key.

  Args:
    data: a dict where every value is a dict
    key_name: the name given to the key that is inlined into the dict's values

  Yields:
    The values from |data|, with each value's key inlined into the value.
  """
  assert isinstance(data, dict), '|data| must be a dict'
  for key, value in data.items():
    assert isinstance(value, dict), '|value| must contain dict items'
    value[key_name] = key
    yield value


def Flatten(data, all_columns):
  """Flattens lists inside |data|, one level deep.

  This function will flatten each dictionary key in |data| into a single row
  so that it can be written to a CSV file.

  Args:
    data: the data to be flattened.
    all_columns: set of all columns that are found in the result (this will be
      filled by the function).

  Yields:
    A list of dict objects whose lists or sets have been flattened.
  """
  SEPARATOR = ', '

  # Max length of a cell in Excel is technically 32767 characters but if we get
  # too close to this limit Excel seems to create weird results when we open
  # the CSV file. To protect against this, give a little more buffer to the max
  # characters.
  MAX_CELL_LENGTH = 32700

  for item in data:
    added_item = {}
    for prop, value in item.items():
      # Non-container properties can be added directly.
      if not isinstance(value, (list, set)):
        added_item[prop] = value
        continue

      # Otherwise join the container together into a single cell.
      num_prop = 'num_' + prop
      added_item[num_prop] = len(value)

      # For long lists, the cell contents may go over MAX_CELL_LENGTH, so
      # split the list into chunks that will fit into MAX_CELL_LENGTH.
      flat_list = SEPARATOR.join(sorted(value))
      overflow_prop_index = 0
      while True:
        current_column = prop
        if overflow_prop_index:
          current_column = prop + '_' + str(overflow_prop_index)

        flat_list_len = len(flat_list)
        if flat_list_len > MAX_CELL_LENGTH:
          last_separator = flat_list.rfind(SEPARATOR, 0,
                                           MAX_CELL_LENGTH - flat_list_len)
          if last_separator != -1:
            added_item[current_column] = flat_list[0:last_separator]
            flat_list = flat_list[last_separator + 2:]
            overflow_prop_index = overflow_prop_index + 1
            continue

        # Fall-through case where no more splitting is possible, this is the
        # lass cell to add for this list.
        added_item[current_column] = flat_list
        break

      assert isinstance(
          added_item[prop],
          (int, bool, str, unicode)), ('unexpected type for item: %s' %
                                       type(added_item[prop]).__name__)

    all_columns.update(added_item.keys())
    yield added_item


def ExtensionListAsCsv(extensions_list, csv_filename, sort_column='name'):
  """Saves an extensions list to a CSV file.

  Args:
    extensions_list: an extensions list as returned by ComputeExtensionsList
    csv_filename: the name of the CSV file to save
    sort_column: the name of the column by which to sort the data
  """
  all_columns = set()
  flattened_list = [
      x for x in ToUtf8(Flatten(DictToList(extensions_list), all_columns))
  ]
  desired_column_order = [
      'id', 'name', 'num_permissions', 'num_installed', 'num_disabled',
      'num_forced', 'permissions', 'installed', 'disabled', 'forced'
  ]

  # Order the columns as desired. Columns other than those in
  # |desired_column_order| will be in an unspecified order after these columns.
  ordered_fieldnames = []
  for c in desired_column_order:
    matching_columns = []
    for f in all_columns:
      if f == c or f.startswith(c):
        matching_columns.append(f)
    ordered_fieldnames.extend(sorted(matching_columns))

  ordered_fieldnames.extend(
      [x for x in desired_column_order if x not in ordered_fieldnames])
  with open(csv_filename, mode='w') as csv_file:
    writer = csv.DictWriter(csv_file, fieldnames=ordered_fieldnames)
    writer.writeheader()
    for row in sorted(flattened_list, key=lambda ext: ext[sort_column]):
      writer.writerow(row)


def main(args):
  # Load the json format key that you downloaded from the Google API
  # Console when you created your service account. For p12 keys, use the
  # from_p12_keyfile method of ServiceAccountCredentials and specify the
  # service account email address, p12 keyfile, and scopes.
  service_credentials = Credentials.from_service_account_file(
      args.service_account_key_path,
      scopes=[
          'https://www.googleapis.com/auth/admin.directory.device.chromebrowsers.readonly'
      ],
      subject=args.admin_email)

  try:
    http = google_auth_httplib2.AuthorizedHttp(service_credentials, http=Http())
    extensions_list = {}
    base_request_url = 'https://admin.googleapis.com/admin/directory/v1.1beta1/customer/my_customer/devices/chromebrowsers'
    request_parameters = ''
    browsers_processed = 0
    while True:
      print('Making request to server ...')
      retrycount = 0
      while retrycount < 5:
        data = json.loads(
            http.request(base_request_url + '?' + request_parameters, 'GET')[1])

        if 'browsers' not in data:
          print('Response error, retrying...')
          time.sleep(3)
          retrycount += 1
        else:
          break

      browsers_in_data = len(data['browsers'])
      print('Request returned %s results, analyzing ...' % (browsers_in_data))
      ComputeExtensionsList(extensions_list, data)
      browsers_processed += browsers_in_data

      if 'nextPageToken' not in data or not data['nextPageToken']:
        break

      print('%s browsers processed.' % (browsers_processed))

      if (args.max_browsers_to_process is not None and
          args.max_browsers_to_process <= browsers_processed):
        print('Stopping at %s browsers processed.' % (browsers_processed))
        break

      request_parameters = ('pageToken={}').format(data['nextPageToken'])
  finally:
    print('Analyze results ...')
    ExtensionListAsCsv(extensions_list, args.extension_list_csv)
    print("Results written to '%s'" % (args.extension_list_csv))


if __name__ == '__main__':
  parser = argparse.ArgumentParser(description='CBCM Extension Analyzer')
  parser.add_argument(
      '-k',
      '--service_account_key_path',
      metavar='FILENAME',
      required=True,
      help='The service account key file used to make API requests.')
  parser.add_argument(
      '-a',
      '--admin_email',
      required=True,
      help='The admin user used to make the API requests.')
  parser.add_argument(
      '-x',
      '--extension_list_csv',
      metavar='FILENAME',
      default='./extension_list.csv',
      help='Generate an extension list to the specified CSV '
      'file')
  parser.add_argument(
      '-m',
      '--max_browsers_to_process',
      type=int,
      help='Maximum number of browsers to process. (Must be > 0).')
  args = parser.parse_args()

  if (args.max_browsers_to_process is not None and
      args.max_browsers_to_process <= 0):
    print('max_browsers_to_process must be > 0.')
    parser.print_help()
    sys.exit(1)

  main(args)
