(function() {
  class CookieHelper {
    constructor(dp) {
      this._dp = dp;
    }

    async getCookiesLog(opt_data) {
      const data = opt_data || (await this._dp.Network.getAllCookies()).result;
      data.cookies.sort((a, b) => a.name.localeCompare(b.name));
      const logs = ['Num of cookies ' + data.cookies.length];
      for (const cookie of data.cookies) {
        let suffix = ''
        if (cookie.partitionKeyOpaque)
          suffix += `, partitionKey: <opaque>`;
        else if (cookie.partitionKey)
        suffix += `, partitionKey: ${JSON.stringify(cookie.partitionKey)}`;
        if (cookie.secure)
          suffix += `, secure`;
        if (cookie.httpOnly)
          suffix += `, httpOnly`;
        if (cookie.session)
          suffix += `, session`;
        if (cookie.sameSite)
          suffix += `, ${cookie.sameSite}`;
        if (cookie.expires !== -1)
          suffix += `, expires`;
        logs.push(`name: ${cookie.name}, value: ${cookie.value}, domain: ${cookie.domain}, path: ${cookie.path}${suffix}`);
      }
      return logs.join('\n');
    }
  };

  return (dp) => {
    return new CookieHelper(dp);
  };
})()
