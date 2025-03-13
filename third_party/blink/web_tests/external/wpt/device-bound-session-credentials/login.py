import json
import importlib
session_manager = importlib.import_module('device-bound-session-credentials.session_manager')

def main(request, response):
    registration_url = "/device-bound-session-credentials/start_session.py"
    request_body_raw = request.body.decode('utf-8')
    if len(request_body_raw) > 0:
        request_body = json.loads(request_body_raw)
        maybe_registration_url = request_body.get("registrationUrl")
        if maybe_registration_url is not None:
            registration_url = maybe_registration_url

    authorization_value = session_manager.find_for_request(request).get_authorization_value()
    authorization_header = ""
    if authorization_value is not None:
        authorization_header = ';authorization="' + authorization_value + '"'

    headers = [('Sec-Session-Registration', '(RS256);challenge="login_challenge_value";path="' + registration_url + '"' + authorization_header)]
    return (200, headers, "")
