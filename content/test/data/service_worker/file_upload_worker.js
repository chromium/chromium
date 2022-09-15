// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Service worker for the file upload test. It responds to a POST submission
// with an HTML document whose body describes the received form data in JSON.

function describeValue(value) {
  const result = {};
  if (value instanceof File) {
    return {type: 'file', size: value.size, name: value.name};
  } else if (value instanceof Blob) {
    return {type: 'blob', size: value.size};
  } else {
    return {type: 'string', data: value};
  }
}

function extractBoundary(request) {
  const reg = new RegExp('multipart\/form-data; boundary=(.*)');
  for (var header of request.headers) {
    if (header[0] == 'content-type') {
      var regResult = reg.exec(header[1]);
      if (regResult)
        return regResult[1];
    }
  }
  return undefined;
}

async function asFormData(request) {
  const formData = await request.formData();
  const result = {};
  result.entries = [];
  for (var pair of formData.entries()) {
    result.entries.push({key: pair[0], value: describeValue(pair[1])});
  }
  return JSON.stringify(result);
}

async function asText(request) {
  const boundary = extractBoundary(request);
  if (!boundary)
    throw 'error: no boundary found';

  const text = await request.text();
  return JSON.stringify({
    boundary: boundary,
    body: text
  });
}

async function asBlob(request) {
  const boundary = extractBoundary(request);
  if (!boundary)
    throw 'error: no boundary found';

  const blob = await request.blob();
  return JSON.stringify({
    boundary: boundary,
    bodySize: blob.size
  });
}

async function generateResponse(request, getAs) {
  let resultString;
  if (getAs == 'formData')
    resultString = await asFormData(request);
  else if (getAs == 'blob')
    resultString = await asBlob(request);
  else
    resultString = await asText(request);

  const body = String.raw`
    <!doctype html>
    <html>
    <title>form submitted</title>
    <body>${resultString}</body>
    </html>
  `;
  const headers = {'content-type': 'text/html'};
  return new Response(body, {headers});
}

self.addEventListener('fetch', event => {
  if (event.request.method != 'POST')
    return;
  const url = new URL(event.request.url);
  const getAs = url.searchParams.get('getAs');
  if (getAs == 'fallback')
    return;
  event.respondWith(generateResponse(event.request, getAs));
});
