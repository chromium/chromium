from webdriver.bidi.modules.storage import StorageKeyPartitionDescriptor
from .. import any_int, recursive_compare


async def assert_cookie_is_set(bidi_session, name: str, str_value: str, domain: str, origin: str, path: str = "/"):
    """
    Asserts the cookie is set.
    """
    all_cookies = await bidi_session.storage.get_cookies(partition=StorageKeyPartitionDescriptor(
        source_origin=origin))

    assert 'cookies' in all_cookies
    cookie = next(c for c in all_cookies['cookies'] if c['name'] == name)

    recursive_compare({
        'domain': domain,
        'httpOnly': False,
        'name': name,
        'path': path,
        'sameSite': 'none',
        'secure': True,
        # Varies depending on the cookie name and value.
        'size': any_int,
        'value': {
            'type': 'string',
            'value': str_value,
        },
    }, cookie)
