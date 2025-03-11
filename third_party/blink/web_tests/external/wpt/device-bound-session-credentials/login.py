import json
import importlib
session_manager = importlib.import_module('device-bound-session-credentials.session_manager')

def main(request, response):
    request_body_raw = request.body.decode('utf-8')
    request_body = json.loads(request_body_raw) if len(request_body_raw) > 0 else None
    num_sessions = request_body.get("numSessions") if request_body is not None else 1
    use_single_header = request_body.get("useSingleHeader") if request_body is not None else True

    authorization_value = session_manager.find_for_request(request).get_authorization_value()
    authorization_header = ""
    if authorization_value is not None:
        authorization_header = ';authorization="' + authorization_value + '"'

    registrations = []
    for i in range(num_sessions):
        registrations.append(('Sec-Session-Registration', '(RS256);challenge="login_challenge_value";path="/device-bound-session-credentials/start_session.py"' + authorization_header))

    if use_single_header:
        combined_registrations = [("Sec-Session-Registration", ", ".join([registration[1] for registration in registrations]))]
        return (200, combined_registrations, "")
    else:
        return (200, registrations, "")
