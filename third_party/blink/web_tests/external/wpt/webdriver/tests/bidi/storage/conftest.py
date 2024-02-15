from urllib.parse import urlunsplit

import pytest
import pytest_asyncio
from webdriver.bidi.modules.script import ContextTarget


@pytest_asyncio.fixture
async def add_cookie(bidi_session):
    """
    Add a cookie with `document.cookie` and remove them after the test is finished.
    """
    cookies = []

    async def add_cookie(
        context,
        name,
        value,
        domain=None,
        expiry=None,
        path=None,
        same_site="none",
        secure=False,
    ):
        cookie_string = f"{name}={value}"
        cookie = {"name": name, "context": context}

        if domain is not None:
            cookie_string += f";domain={domain}"

        if expiry is not None:
            cookie_string += f";expires={expiry}"

        if path is not None:
            cookie_string += f";path={path}"
            cookie["path"] = path

        if same_site != "none":
            cookie_string += f";SameSite={same_site}"

        if secure is True:
            cookie_string += ";Secure"

        await bidi_session.script.evaluate(
            expression=f"document.cookie = '{cookie_string}'",
            target=ContextTarget(context),
            await_promise=True,
        )

        cookies.append(cookie)

    yield add_cookie

    for cookie in reversed(cookies):
        cookie_string = f"""{cookie["name"]}="""

        if "path" in cookie:
            cookie_string += f""";path={cookie["path"]}"""

        await bidi_session.script.evaluate(
            expression=f"""document.cookie = '{cookie_string};Max-Age=0'""",
            target=ContextTarget(cookie["context"]),
            await_promise=True,
        )


@pytest.fixture
def origin(server_config, domain_value):
    def origin(protocol="https", domain="", subdomain=""):
        return urlunsplit((protocol, domain_value(domain, subdomain), "", "", ""))

    return origin


@pytest.fixture
def domain_value(server_config):
    def domain_value(domain="", subdomain=""):
        return server_config["domains"][domain][subdomain]

    return domain_value
