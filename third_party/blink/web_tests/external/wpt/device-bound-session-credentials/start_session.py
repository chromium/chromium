import importlib
jwt_helper = importlib.import_module('device-bound-session-credentials.jwt_helper')
session_manager = importlib.import_module('device-bound-session-credentials.session_manager')

def main(request, response):
    jwt_header, jwt_payload, verified = jwt_helper.decode_jwt(request.headers.get("Sec-Session-Response").decode('utf-8'))
    test_session_manager = session_manager.find_for_request(request)
    session_id = test_session_manager.create_new_session()
    test_session_manager.set_session_key(session_id, jwt_payload.get('key'))

    if not verified or jwt_payload.get("jti") != "login_challenge_value":
        return (400, response.headers, "")

    if jwt_payload.get("authorization") != test_session_manager.get_authorization_value():
        return (400, response.headers, "")

    return test_session_manager.get_session_instructions_response(session_id, request)
