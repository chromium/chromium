from typing import Optional, Tuple
from urllib.parse import urlsplit, parse_qsl


def session_established(session):
    path: Optional[bytes] = None
    for key, value in session.request_headers:
        if key == b':path':
            path = value
    assert path is not None
    qs = dict(parse_qsl(urlsplit(path).query))
    token = qs[b'token']
    if token is None:
        raise Exception('token is missing, path = {}'.format(path))
    session.dict_for_handlers['token'] = token


def session_closed(
        session, close_info: Optional[Tuple[int, bytes]], abruptly: bool) -> None:
    token = session.dict_for_handlers['token']
    data = session.stash.take(key=token) or {}

    data['session-close-info'] = {
        'abruptly': abruptly,
        'close_info': close_info
    }
    session.stash.put(key=token, value=data)