def main(request, response):
  if len(request.cookies) > 0:
    return (530, [], "Cookie should not be sent to this endpoint")
  if request.headers.get(b"Accept") != b"application/json":
    return (531, [], "Wrong Accept")
  if request.headers.get(b"Sec-Fetch-Dest") != b"webidentity":
    return (532, [], "Wrong Sec-Fetch-Dest header")
  if not request.headers.get(b"Referer"):
    return (533, [], "Missing Referer")

  return """
{
  "privacy_policy_url": "https://privacypolicy.com"
}
"""
