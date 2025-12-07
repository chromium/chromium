# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Code for interacting with BigQuery."""

from typing import Generator

# pylint: disable=import-error
from google.cloud import bigquery
from google.cloud import bigquery_storage
import pandas
# pylint: enable=import-error


class Querier:

  def __init__(self, billing_project: str):
    self._billing_project = billing_project

  def GetSeriesForQuery(self,
                        query: str) -> Generator[pandas.Series, None, None]:
    """Generates results for |query|.

    Args:
      query: The BigQuery query to run.

    Yields:
      A pandas.Series object for each row returned by the query. Columns can be
      accessed directly as attributes.
    """
    client = bigquery.Client(project=self._billing_project)
    job = client.query(query)
    row_iterator = job.result()
    # Using a Dataframe iterator instead of directly using |row_iterator| allows
    # us to use the BigQuery Storage API, which results in ~10x faster query
    # result retrieval at the cost of a few more dependencies.
    dataframe_iterator = row_iterator.to_dataframe_iterable(
        bigquery_storage.BigQueryReadClient())
    for df in dataframe_iterator:
      for _, row in df.iterrows():
        yield row
