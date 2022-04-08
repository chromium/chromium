# Copyright 2022 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging


def _output_local_diff_files(skia_gold_session, image_name):
  """Logs the local diff image files from the given SkiaGoldSession

  Args:
    skia_gold_session: A SkiaGoldSession instance to pull files
        from.
    image_name: A string containing the name of the image/test that was
        compared.

  Returns:
    None
  """
  given_file = skia_gold_session.GetGivenImageLink(image_name)
  closest_file = skia_gold_session.GetClosestImageLink(image_name)
  diff_file = skia_gold_session.GetDiffImageLink(image_name)
  failure_message = 'Unable to retrieve link'
  logging.error('Generated image: %s', given_file or failure_message)
  logging.error('Closest image: %s', closest_file or failure_message)
  logging.error('Diff image: %s', diff_file or failure_message)


def log_skia_gold_status_code(skia_gold_session, image_name, status, error):
  """Checks the skia gold status code and logs more detailed message

  Args:
    skia_gold_session: A SkiaGoldSession object.
    image_name: The name of the image file.
    status: A StatusCodes returned from RunComparison.
    error: An error message describing the status if not successful

  Returns:
    A link to a triage page if there are images to triage, otherwise None
  """
  triage_link = None
  status_codes = skia_gold_session.StatusCodes
  if status in (status_codes.AUTH_FAILURE, status_codes.INIT_FAILURE):
    logging.error('Gold failed with code %d output %s', status, error)
  elif status == status_codes.COMPARISON_FAILURE_REMOTE:
    _, triage_link = skia_gold_session.GetTriageLinks(image_name)
    if not triage_link:
      logging.error('Failed to get triage link for %s, raw output: %s',
                    image_name, error)
      logging.error('Reason for no triage link: %s',
                    skia_gold_session.GetTriageLinkOmissionReason(image_name))
    else:
      logging.warning('triage link: %s', triage_link)
  elif status == status_codes.COMPARISON_FAILURE_LOCAL:
    logging.error('Local comparison failed. Local diff files:')
    _output_local_diff_files(skia_gold_session, image_name)
  elif status == status_codes.LOCAL_DIFF_FAILURE:
    logging.error(
        'Local comparison failed and an error occurred during diff '
        'generation: %s', error)
    # There might be some files, so try outputting them.
    logging.error('Local diff files:')
    _output_local_diff_files(skia_gold_session, image_name)
  else:
    logging.error(
        'Given unhandled SkiaGoldSession StatusCode %s with error %s', status,
        error)
  return triage_link

