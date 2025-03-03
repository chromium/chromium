import json

import importlib
session_manager = importlib.import_module('device-bound-session-credentials.session_manager')

def main(request, response):
    request_body = json.loads(request.body.decode('utf-8'))
    test_session_manager = session_manager.find_for_request(request)

    should_refresh_end_session = request_body.get("shouldRefreshEndSession")
    if should_refresh_end_session is not None:
        test_session_manager.set_should_refresh_end_session(should_refresh_end_session)

    authorization_value = request_body.get("authorizationValue")
    if authorization_value is not None:
        test_session_manager.set_authorization_value(authorization_value)

    send_challenge_early = request_body.get("sendChallengeEarly")
    if send_challenge_early is not None:
        test_session_manager.set_send_challenge_early(send_challenge_early)

    return (200, response.headers, "")
