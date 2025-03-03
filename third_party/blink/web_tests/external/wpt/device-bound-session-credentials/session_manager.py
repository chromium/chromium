import json

test_to_session_manager_mapping = {}

def initialize_test():
    test_id = str(len(test_to_session_manager_mapping))
    test_to_session_manager_mapping[test_id] = SessionManager()
    return test_id

def find_for_request(request):
    test_id = request.cookies.get(b'test_id').value.decode('utf-8')
    manager = test_to_session_manager_mapping.get(test_id)
    if manager == None:
        raise Exception("Could not find manager for test_id: " + test_id)
    return manager

class SessionManager:
    def __init__(self):
        self.session_to_key_map = {}
        self.should_refresh_end_session = False
        self.authorization_value = None
        self.send_challenge_early = False

    def create_new_session(self):
        session_id = str(len(self.session_to_key_map))
        self.session_to_key_map[session_id] = None
        return session_id

    def set_session_key(self, session_id, key):
        if session_id not in self.session_to_key_map:
            return False
        self.session_to_key_map[session_id] = key
        return True

    def get_session_key(self, session_id):
        return self.session_to_key_map.get(session_id)

    def get_session_ids(self):
        return list(self.session_to_key_map.keys())

    def set_should_refresh_end_session(self, value):
        self.should_refresh_end_session = value

    def get_should_refresh_end_session(self):
        return self.should_refresh_end_session

    def get_authorization_value(self):
        return self.authorization_value

    def set_authorization_value(self, value):
        self.authorization_value = value

    def set_send_challenge_early(self, value):
        self.send_challenge_early = value

    def get_send_challenge_early(self):
        return self.send_challenge_early

    def get_session_instructions_response(self, session_id, request):
        response_body = {
            "session_identifier": session_id,
            "refresh_url": "/device-bound-session-credentials/refresh_session.py",
            "scope": {
                "include_site": True,
                "scope_specification" : [
                    { "type": "exclude", "domain": request.url_parts.hostname, "path": "/device-bound-session-credentials/request_early_challenge.py" },
                    { "type": "exclude", "domain": request.url_parts.hostname, "path": "/device-bound-session-credentials/end_session_via_clear_site_data.py" },
                ]
            },
            "credentials": [{
                "type": "cookie",
                "name": "auth_cookie",
                "attributes": "Domain=" + request.url_parts.hostname + "; Path=/device-bound-session-credentials"
            }]
        }
        headers = [
            ("Content-Type", "application/json"),
            ("Cache-Control", "no-store"),
            ("Set-Cookie", "auth_cookie=abcdef0123; Domain=" + request.url_parts.hostname + "; Path=/device-bound-session-credentials")
        ]
        return (200, headers, json.dumps(response_body))
