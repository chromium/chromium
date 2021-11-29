# This endpoint responds to both preflight requests and the subsequent requests.
#
# Its behavior can be configured with the following search/GET parameters:
#
# - preflight-uuid: Optional, must be a valid UUID if set.
#   If set, then this endpoint expects to receive a preflight request first
#   followed by a regular request, as in the regular CORS protocol.
#   If unset, then this endpoint expects to receive no preflight request, only
#   a regular (non-OPTIONS) request.
# - preflight-headers: Optional, valid values are:
#   - cors: this endpoint responds with valid CORS headers to preflights. These
#     should be sufficient for non-PNA preflight requests to succeed, but not
#     for PNA-specific preflight requests.
#   - cors+pna: this endpoint responds with valid CORS and PNA headers to
#     preflights. These should be sufficient for both non-PNA preflight
#     requests and PNA-specific preflight requests to succeed.
#   - unspecified, or any other value: this endpoint responds with no CORS or
#     PNA headers. Preflight requests should fail.
# - final-headers: Optional, valid values are:
#   - cors: this endpoint responds with valid CORS headers to CORS-enabled
#     non-preflight requests. These should be sufficient for non-preflighted
#     CORS-enabled requests to succeed.
#   - unspecified: this endpoint responds with no CORS headers to non-preflight
#     requests. This should fail CORS-enabled requests, but be sufficient for
#     no-CORS requests.
#

_ACAO = ("Access-Control-Allow-Origin", "*")
_ACAPN = ("Access-Control-Allow-Private-Network", "true")

def _get_response_headers(method, mode):
  acam = ("Access-Control-Allow-Methods", method)

  if mode == b"cors":
    return [acam, _ACAO]

  if mode == b"cors+pna":
    return [acam, _ACAO, _ACAPN]

  return []

def _get_uuid(request):
  return request.GET.get(b"preflight-uuid")

def _handle_preflight_request(request, response):
  uuid = _get_uuid(request)
  if uuid is None:
    raise Exception("missing `preflight-uuid` param from preflight URL")

  request.server.stash.put(uuid, "")

  method = request.headers.get("Access-Control-Request-Method")
  mode = request.GET.get(b"preflight-headers")
  headers = _get_response_headers(method, mode)

  return (headers, "preflight")

def _handle_final_request(request, response):
  uuid = _get_uuid(request)
  if uuid is not None and request.server.stash.take(uuid) is None:
    raise Exception("no matching preflight request for {}".format(uuid))

  mode = request.GET.get(b"final-headers")
  headers = _get_response_headers(request.method, mode)

  return (headers, "success")

def main(request, response):
  try:
    if request.method == "OPTIONS":
      return _handle_preflight_request(request, response)
    else:
      return _handle_final_request(request, response)
  except BaseException as e:
    # Surface exceptions to the client, where they show up as assertion errors.
    return ([("X-exception", str(e))], "")#"exception: {}".format(e))
