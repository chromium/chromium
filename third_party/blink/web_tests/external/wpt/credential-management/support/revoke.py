def main(request, response):
  if not b"hint" in request.POST:
    return (500, [], "Missing hint")
  if request.POST[b"hint"] == b"fail":
    return (500, [], "Fail requested")
  return (204, [], "")
